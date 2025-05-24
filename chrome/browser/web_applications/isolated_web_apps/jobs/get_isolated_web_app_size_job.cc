// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/jobs/get_isolated_web_app_size_job.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <variant>

#include "base/barrier_closure.h"
#include "base/check_deref.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/web_applications/commands/command_result.h"
#include "chrome/browser/web_applications/commands/computed_app_size.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/locks/with_app_resources.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/browsing_data/content/browsing_data_model.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/storage_partition_config.h"
#include "ui/base/models/tree_model.h"
#include "url/origin.h"

namespace web_app {

namespace {
const char kDebugOriginKey[] = "iwa_origins";

// Estimates the size in bytes of a non-default StoragePartition by summing the
// size of all browsing data stored within it.
class StoragePartitionSizeEstimator : private ProfileObserver {
 public:
  static void EstimateSize(
      Profile* profile,
      const content::StoragePartitionConfig& storage_partition_config,
      base::OnceCallback<void(int64_t)> complete_callback) {
    DCHECK(!storage_partition_config.is_default());

    // |owning_closure| will own the StoragePartitionSizeEstimator and delete
    // it when called or reset.
    auto* estimator = new StoragePartitionSizeEstimator(profile);
    base::OnceClosure owning_closure =
        base::BindOnce(&DeleteEstimatorSoon, base::WrapUnique(estimator));

    estimator->Start(
        storage_partition_config,
        std::move(complete_callback).Then(std::move(owning_closure)));
  }

 private:
  static void DeleteEstimatorSoon(
      std::unique_ptr<StoragePartitionSizeEstimator> estimator) {
    estimator->profile_->RemoveObserver(estimator.get());
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(estimator));
  }

  explicit StoragePartitionSizeEstimator(Profile* profile) : profile_(profile) {
    profile_->AddObserver(this);
  }

  void Start(const content::StoragePartitionConfig& storage_partition_config,
             base::OnceCallback<void(int64_t)> complete_callback) {
    complete_callback_ = std::move(complete_callback);
    content::StoragePartition* storage_partition =
        profile_->GetStoragePartition(storage_partition_config);
    BrowsingDataModel::BuildFromNonDefaultStoragePartition(
        storage_partition,
        ChromeBrowsingDataModelDelegate::CreateForStoragePartition(
            profile_, storage_partition),
        base::BindOnce(&StoragePartitionSizeEstimator::BrowsingDataModelLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void BrowsingDataModelLoaded(
      std::unique_ptr<BrowsingDataModel> browsing_data_model) {
    browsing_data_model_ = std::move(browsing_data_model);

    int64_t size = 0;
    for (const BrowsingDataModel::BrowsingDataEntryView& entry :
         *browsing_data_model_) {
      size += entry.data_details->storage_size;
    }
    std::move(complete_callback_).Run(size);
  }

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override {
    // Abort if the Profile is being deleted. |complete_callback_| owns the
    // object, so resetting it will delete |this|.
    complete_callback_.Reset();
  }

  raw_ptr<Profile> profile_;
  base::OnceCallback<void(int64_t)> complete_callback_;
  std::unique_ptr<BrowsingDataModel> browsing_data_model_;
  base::WeakPtrFactory<StoragePartitionSizeEstimator> weak_ptr_factory_{this};
};

}  // namespace

GetIsolatedWebAppSizeJob::GetIsolatedWebAppSizeJob(
    Profile* profile,
    const webapps::AppId& app_id,
    base::Value::Dict& debug_value,
    ResultCallback result_callback)
    : app_id_(app_id),
      profile_(profile),
      debug_value_(debug_value),
      result_callback_(std::move(result_callback)) {
  CHECK(profile_);
  debug_value_->Set("profile", profile->GetDebugName());
}

GetIsolatedWebAppSizeJob::~GetIsolatedWebAppSizeJob() = default;

void GetIsolatedWebAppSizeJob::Start(
    WithAppResources* lock_with_app_resources) {
  CHECK(lock_with_app_resources);
  lock_with_app_resources_ = lock_with_app_resources;

  const WebAppRegistrar& web_app_registrar =
      lock_with_app_resources_->registrar();
  const WebApp iwa = CHECK_DEREF(web_app_registrar.GetAppById(app_id_));
  CHECK(iwa.isolation_data());

  iwa_origin_ = url::Origin::Create(iwa.scope());

  base::ConcurrentClosures barrier;

  lock_with_app_resources_->icon_manager().GetIconsSizeForApp(
      app_id_, base::BindOnce(&GetIsolatedWebAppSizeJob::OnGetIconStorageUsage,
                              weak_factory_.GetWeakPtr())
                   .Then(barrier.CreateClosure()));
  for (const content::StoragePartitionConfig& storage_partition_config :
       web_app_registrar.GetIsolatedWebAppStoragePartitionConfigs(app_id_)) {
    if (storage_partition_config.in_memory()) {
      continue;
    }

    debug_value_->EnsureDict(kDebugOriginKey)->Set(iwa_origin_.Serialize(), -1);
    StoragePartitionSizeEstimator::EstimateSize(
        profile_, storage_partition_config,
        base::BindOnce(&GetIsolatedWebAppSizeJob::StoragePartitionSizeFetched,
                       weak_factory_.GetWeakPtr())
            .Then(barrier.CreateClosure()));
  }

  std::visit(
      base::Overloaded{
          [&](const IwaStorageOwnedBundle& owned_bundle) {
            base::ThreadPool::PostTaskAndReplyWithResult(
                FROM_HERE, {base::MayBlock()},
                base::GetFileSizeCallback(
                    owned_bundle.GetPath(profile_->GetPath())),
                base::BindOnce(&GetIsolatedWebAppSizeJob::OnBundleSizeComputed,
                               weak_factory_.GetWeakPtr())
                    .Then(barrier.CreateClosure()));
          },
          [&](const auto&) { OnBundleSizeComputed(/*bundle_size=*/0u); }},
      iwa.isolation_data()->location().variant());

  std::move(barrier).Done(base::BindOnce(&GetIsolatedWebAppSizeJob::CompleteJob,
                                         weak_factory_.GetWeakPtr()));
}

void GetIsolatedWebAppSizeJob::OnGetIconStorageUsage(uint64_t icon_size) {
  icon_size_ = icon_size;
  debug_value_->Set("app_icon_size_in_bytes", base::ToString(icon_size));
}

void GetIsolatedWebAppSizeJob::StoragePartitionSizeFetched(int64_t size) {
  browsing_data_size_ += size;
  // Store the size as a double because Value::Dict doesn't support 64-bit
  // integers. This should only lead to data loss when size is >2^54.
  debug_value_->EnsureDict(kDebugOriginKey)
      ->Set(iwa_origin_.Serialize(), base::ToString(size));
}

void GetIsolatedWebAppSizeJob::OnBundleSizeComputed(
    std::optional<int64_t> bundle_size) {
  if (!bundle_size) {
    // optional remains empty -- it will be treated as an error condition in
    // CompleteJob().
    return;
  }
  bundle_size_ = bundle_size;
}

void GetIsolatedWebAppSizeJob::CompleteJobWithError() {
  std::move(result_callback_).Run(/*result=*/std::nullopt);
}

void GetIsolatedWebAppSizeJob::CompleteJob() {
  if (!bundle_size_) {
    std::move(result_callback_).Run(std::nullopt);
    return;
  }

  debug_value_->Set(
      "total_app_size_in_bytes",
      base::ToString(*bundle_size_ + icon_size_ + browsing_data_size_));
  debug_value_->Set("bundle_size_in_bytes", base::ToString(bundle_size_));
  debug_value_->Set("browsing_data_size_in_bytes",
                    base::ToString(browsing_data_size_));

  std::move(result_callback_)
      .Run(web_app::ComputedAppSizeWithOrigin(
          *bundle_size_ + static_cast<uint64_t>(icon_size_),
          browsing_data_size_, iwa_origin_));
}

}  // namespace web_app

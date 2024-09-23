// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/get_isolated_web_app_browsing_data_command.h"

#include <memory>

#include "base/barrier_closure.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
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

  raw_ptr<Profile> profile_ = nullptr;
  base::OnceCallback<void(int64_t)> complete_callback_;
  std::unique_ptr<BrowsingDataModel> browsing_data_model_;
  base::WeakPtrFactory<StoragePartitionSizeEstimator> weak_ptr_factory_{this};
};

}  // namespace

GetIsolatedWebAppBrowsingDataCommand::GetIsolatedWebAppBrowsingDataCommand(
    Profile* profile,
    BrowsingDataCallback callback)
    : WebAppCommand<AllAppsLock, base::flat_map<url::Origin, int64_t>>(
          "GetIsolatedWebAppBrowsingDataCommand",
          AllAppsLockDescription(),
          std::move(callback),
          /*args_for_shutdown=*/{}),
      profile_(profile),
      browsing_data_({}) {
  GetMutableDebugValue().Set("profile", profile_->GetDebugName());
}

GetIsolatedWebAppBrowsingDataCommand::~GetIsolatedWebAppBrowsingDataCommand() =
    default;

void GetIsolatedWebAppBrowsingDataCommand::StartWithLock(
    std::unique_ptr<AllAppsLock> lock) {
  lock_ = std::move(lock);

  pending_task_count_++;
  const WebAppRegistrar& web_app_registrar = lock_->registrar();
  for (const WebApp& web_app : web_app_registrar.GetApps()) {
    const webapps::AppId& app_id = web_app.app_id();
    if (!web_app_registrar.IsIsolated(app_id)) {
      continue;
    }
    url::Origin iwa_origin = url::Origin::Create(web_app.scope());
    for (const content::StoragePartitionConfig& storage_partition_config :
         web_app_registrar.GetIsolatedWebAppStoragePartitionConfigs(app_id)) {
      if (storage_partition_config.in_memory()) {
        continue;
      }
      pending_task_count_++;
      GetMutableDebugValue()
          .EnsureDict(kDebugOriginKey)
          ->Set(iwa_origin.Serialize(), -1);
      StoragePartitionSizeEstimator::EstimateSize(
          profile_, storage_partition_config,
          base::BindOnce(&GetIsolatedWebAppBrowsingDataCommand::
                             StoragePartitionSizeFetched,
                         weak_factory_.GetWeakPtr(),
                         /*data_key=*/iwa_origin));
    }
  }
  pending_task_count_--;

  MaybeCompleteCommand();
}

void GetIsolatedWebAppBrowsingDataCommand::StoragePartitionSizeFetched(
    const url::Origin& iwa_origin,
    int64_t size) {
  DCHECK_GT(pending_task_count_, 0);
  pending_task_count_--;
  browsing_data_[iwa_origin] += size;
  // Store the size as a double because Value::Dict doesn't support 64-bit
  // integers. This should only lead to data loss when size is >2^54.
  GetMutableDebugValue()
      .EnsureDict(kDebugOriginKey)
      ->Set(iwa_origin.Serialize(), static_cast<double>(size));

  MaybeCompleteCommand();
}

void GetIsolatedWebAppBrowsingDataCommand::MaybeCompleteCommand() {
  if (pending_task_count_ == 0) {
    CompleteAndSelfDestruct(CommandResult::kSuccess, browsing_data_);
  }
}

}  // namespace web_app

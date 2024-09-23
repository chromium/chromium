// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/core/power_bookmark_service.h"

#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/power_bookmarks/common/power.h"
#include "components/power_bookmarks/common/power_overview.h"
#include "components/power_bookmarks/common/search_params.h"
#include "components/power_bookmarks/core/power_bookmark_data_provider.h"
#include "components/power_bookmarks/core/power_bookmark_features.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/storage/power_bookmark_backend.h"
#include "components/sync/model/proxy_data_type_controller_delegate.h"
#include "components/sync/protocol/power_bookmark_specifics.pb.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace power_bookmarks {

PowerBookmarkService::PowerBookmarkService(
    BookmarkModel* model,
    const base::FilePath& database_dir,
    scoped_refptr<base::SequencedTaskRunner> frontend_task_runner,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : model_(model), backend_task_runner_(backend_task_runner) {
  if (model_)
    model_->AddObserver(this);

  backend_ = std::make_unique<PowerBookmarkBackend>(
      database_dir, frontend_task_runner, weak_ptr_factory_.GetWeakPtr());
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PowerBookmarkBackend::Init,
                     base::Unretained(backend_.get()),
                     base::FeatureList::IsEnabled(kPowerBookmarkBackend)));
}

PowerBookmarkService::~PowerBookmarkService() {
  if (model_)
    model_->RemoveObserver(this);

  backend_task_runner_->DeleteSoon(FROM_HERE, std::move(backend_));
}

std::unique_ptr<syncer::DataTypeControllerDelegate>
PowerBookmarkService::CreateSyncControllerDelegate() {
  return std::make_unique<syncer::ProxyDataTypeControllerDelegate>(
      backend_task_runner_,
      base::BindRepeating(&PowerBookmarkBackend::GetSyncControllerDelegate,
                          base::Unretained(backend_.get())));
}

void PowerBookmarkService::GetPowersForURL(
    const GURL& url,
    const sync_pb::PowerBookmarkSpecifics::PowerType& power_type,
    PowersCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PowerBookmarkBackend::GetPowersForURL,
                     base::Unretained(backend_.get()), url, power_type),
      std::move(callback));
}

void PowerBookmarkService::GetPowerOverviewsForType(
    const sync_pb::PowerBookmarkSpecifics::PowerType& power_type,
    PowerOverviewsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PowerBookmarkBackend::GetPowerOverviewsForType,
                     base::Unretained(backend_.get()), power_type),
      std::move(callback));
}

void PowerBookmarkService::SearchPowers(const SearchParams& search_params,
                                        PowersCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PowerBookmarkBackend::SearchPowers,
                     base::Unretained(backend_.get()), search_params),
      std::move(callback));
}

void PowerBookmarkService::SearchPowerOverviews(
    const SearchParams& search_params,
    PowerOverviewsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PowerBookmarkBackend::SearchPowerOverviews,
                     base::Unretained(backend_.get()), search_params),
      std::move(callback));
}

void PowerBookmarkService::CreatePower(std::unique_ptr<Power> power,
                                       SuccessCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Accept existing guids if they're explicitly set.
  if (!power->guid().is_valid())
    power->set_guid(base::Uuid::GenerateRandomV4());
  base::Time now = base::Time::Now();
  if (power->time_added().is_null())
    power->set_time_added(now);
  if (power->time_modified().is_null())
    power->set_time_modified(now);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PowerBookmarkBackend::CreatePower,
                     base::Unretained(backend_.get()), std::move(power)),
      std::move(callback));
}

void PowerBookmarkService::UpdatePower(std::unique_ptr<Power> power,
                                       SuccessCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  power->set_time_modified(base::Time::Now());
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PowerBookmarkBackend::UpdatePower,
                     base::Unretained(backend_.get()), std::move(power)),
      std::move(callback));
}

void PowerBookmarkService::DeletePower(const base::Uuid& guid,
                                       SuccessCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PowerBookmarkBackend::DeletePower,
                     base::Unretained(backend_.get()), guid),
      std::move(callback));
}

void PowerBookmarkService::DeletePowersForURL(
    const GURL& url,
    const sync_pb::PowerBookmarkSpecifics::PowerType& power_type,
    SuccessCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PowerBookmarkBackend::DeletePowersForURL,
                     base::Unretained(backend_.get()), url, power_type),
      std::move(callback));
}

void PowerBookmarkService::AddObserver(PowerBookmarkObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void PowerBookmarkService::RemoveObserver(PowerBookmarkObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void PowerBookmarkService::AddDataProvider(
    PowerBookmarkDataProvider* data_provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_providers_.emplace_back(data_provider);
}

void PowerBookmarkService::RemoveDataProvider(
    PowerBookmarkDataProvider* data_provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = base::ranges::find(data_providers_, data_provider);
  if (it != data_providers_.end())
    data_providers_.erase(it);
}

void PowerBookmarkService::BookmarkNodeAdded(const BookmarkNode* parent,
                                             size_t index,
                                             bool newly_added) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!newly_added)
    return;

  const BookmarkNode* node = parent->children()[index].get();
  std::unique_ptr<PowerBookmarkMeta> meta =
      std::make_unique<PowerBookmarkMeta>();

  for (power_bookmarks::PowerBookmarkDataProvider* data_provider :
       data_providers_) {
    data_provider->AttachMetadataForNewBookmark(node, meta.get());
  }

  SetNodePowerBookmarkMeta(model_, node, std::move(meta));
}

void PowerBookmarkService::OnPowersChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : observers_) {
    observer.OnPowersChanged();
  }
}

}  // namespace power_bookmarks

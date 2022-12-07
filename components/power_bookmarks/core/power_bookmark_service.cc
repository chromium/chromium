// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/core/power_bookmark_service.h"

#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/power_bookmarks/core/power_bookmark_data_provider.h"
#include "components/power_bookmarks/core/power_bookmark_features.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/powers/power.h"
#include "components/power_bookmarks/core/powers/power_overview.h"
#include "components/power_bookmarks/core/powers/search_params.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/metrics/power_bookmark_metrics.h"
#include "components/power_bookmarks/storage/power_bookmark_backend.h"
#include "components/sync/protocol/power_bookmark_specifics.pb.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace power_bookmarks {

PowerBookmarkService::PowerBookmarkService(
    BookmarkModel* model,
    const base::FilePath& database_dir,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : model_(model), backend_task_runner_(backend_task_runner) {
  if (model_)
    model_->AddObserver(this);

  backend_ = base::SequenceBound<PowerBookmarkBackend>(backend_task_runner_,
                                                       database_dir);
  backend_.AsyncCall(&PowerBookmarkBackend::Init)
      .WithArgs(base::FeatureList::IsEnabled(kPowerBookmarkBackend));
}

PowerBookmarkService::~PowerBookmarkService() {
  if (model_)
    model_->RemoveObserver(this);

  backend_.AsyncCall(&PowerBookmarkBackend::Shutdown);
  backend_task_runner_ = nullptr;
}

void PowerBookmarkService::GetPowersForURL(
    const GURL& url,
    const sync_pb::PowerBookmarkSpecifics::PowerType& power_type,
    PowersCallback callback) {
  backend_.AsyncCall(&PowerBookmarkBackend::GetPowersForURL)
      .WithArgs(url, power_type)
      .Then(std::move(callback));
}

void PowerBookmarkService::GetPowerOverviewsForType(
    const sync_pb::PowerBookmarkSpecifics::PowerType& power_type,
    PowerOverviewsCallback callback) {
  backend_.AsyncCall(&PowerBookmarkBackend::GetPowerOverviewsForType)
      .WithArgs(power_type)
      .Then(std::move(callback));
}

void PowerBookmarkService::Search(const SearchParams& search_params,
                                  PowersCallback callback) {
  backend_.AsyncCall(&PowerBookmarkBackend::Search)
      .WithArgs(search_params)
      .Then(std::move(callback));
}

void PowerBookmarkService::CreatePower(std::unique_ptr<Power> power,
                                       SuccessCallback callback) {
  // Accept existing guids if they're explicitly set.
  if (!power->guid().is_valid())
    power->set_guid(base::GUID::GenerateRandomV4());
  base::Time now = base::Time::Now();
  if (power->time_added().is_null())
    power->set_time_added(now);
  if (power->time_modified().is_null())
    power->set_time_modified(now);
  sync_pb::PowerBookmarkSpecifics::PowerType power_type = power->power_type();
  backend_.AsyncCall(&PowerBookmarkBackend::CreatePower)
      .WithArgs(std::move(power))
      .Then(base::BindOnce(&PowerBookmarkService::NotifyAndRecordPowerCreated,
                           weak_ptr_factory_.GetWeakPtr(), power_type,
                           std::move(callback)));
}

void PowerBookmarkService::NotifyAndRecordPowerCreated(
    sync_pb::PowerBookmarkSpecifics::PowerType power_type,
    SuccessCallback callback,
    bool success) {
  std::move(callback).Run(success);
  NotifyPowersChanged(success);
  metrics::RecordPowerCreated(power_type, success);
}

void PowerBookmarkService::UpdatePower(std::unique_ptr<Power> power,
                                       SuccessCallback callback) {
  power->set_time_modified(base::Time::Now());
  sync_pb::PowerBookmarkSpecifics::PowerType power_type = power->power_type();
  backend_.AsyncCall(&PowerBookmarkBackend::UpdatePower)
      .WithArgs(std::move(power))
      .Then(base::BindOnce(&PowerBookmarkService::NotifyAndRecordPowerUpdated,
                           weak_ptr_factory_.GetWeakPtr(), power_type,
                           std::move(callback)));
}

void PowerBookmarkService::NotifyAndRecordPowerUpdated(
    sync_pb::PowerBookmarkSpecifics::PowerType power_type,
    SuccessCallback callback,
    bool success) {
  std::move(callback).Run(success);
  NotifyPowersChanged(success);
  metrics::RecordPowerUpdated(power_type, success);
}

void PowerBookmarkService::DeletePower(const base::GUID& guid,
                                       SuccessCallback callback) {
  backend_.AsyncCall(&PowerBookmarkBackend::DeletePower)
      .WithArgs(guid)
      .Then(base::BindOnce(&PowerBookmarkService::NotifyAndRecordPowerDeleted,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(callback)));
}

void PowerBookmarkService::NotifyAndRecordPowerDeleted(SuccessCallback callback,
                                                       bool success) {
  std::move(callback).Run(success);
  NotifyPowersChanged(success);
  metrics::RecordPowerDeleted(success);
}

void PowerBookmarkService::DeletePowersForURL(
    const GURL& url,
    const sync_pb::PowerBookmarkSpecifics::PowerType& power_type,
    SuccessCallback callback) {
  backend_.AsyncCall(&PowerBookmarkBackend::DeletePowersForURL)
      .WithArgs(url, power_type)
      .Then(base::BindOnce(
          &PowerBookmarkService::NotifyAndRecordPowersDeletedForURL,
          weak_ptr_factory_.GetWeakPtr(), power_type, std::move(callback)));
}

void PowerBookmarkService::NotifyAndRecordPowersDeletedForURL(
    sync_pb::PowerBookmarkSpecifics::PowerType power_type,
    SuccessCallback callback,
    bool success) {
  std::move(callback).Run(success);
  NotifyPowersChanged(success);
  metrics::RecordPowersDeletedForURL(power_type, success);
}

void PowerBookmarkService::NotifyPowersChanged(bool success) {
  // If the create/update/delete call wasn't successful, then there was no
  // functional change to the backend. In this case, skip notifying observers.
  if (!success)
    return;

  for (auto& observer : observers_)
    observer.OnPowersChanged();
}

void PowerBookmarkService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PowerBookmarkService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PowerBookmarkService::AddDataProvider(
    PowerBookmarkDataProvider* data_provider) {
  data_providers_.emplace_back(data_provider);
}

void PowerBookmarkService::RemoveDataProvider(
    PowerBookmarkDataProvider* data_provider) {
  auto it = base::ranges::find(data_providers_, data_provider);
  if (it != data_providers_.end())
    data_providers_.erase(it);
}

void PowerBookmarkService::BookmarkNodeAdded(BookmarkModel* model,
                                             const BookmarkNode* parent,
                                             size_t index,
                                             bool newly_added) {
  if (!newly_added)
    return;

  const BookmarkNode* node = parent->children()[index].get();
  std::unique_ptr<PowerBookmarkMeta> meta =
      std::make_unique<PowerBookmarkMeta>();

  for (auto* data_provider : data_providers_) {
    data_provider->AttachMetadataForNewBookmark(node, meta.get());
  }

  SetNodePowerBookmarkMeta(model, node, std::move(meta));
}

}  // namespace power_bookmarks

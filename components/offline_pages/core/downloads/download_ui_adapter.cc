// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/downloads/download_ui_adapter.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "components/offline_items_collection/core/fail_state.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/request_notifier.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/downloads/offline_item_conversions.h"
#include "components/offline_pages/core/offline_page_client_policy.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/page_criteria.h"
#include "components/offline_pages/core/visuals_decoder.h"
#include "ui/gfx/image/image.h"

namespace {
// Value of this constant doesn't matter, only its address is used.
const char kDownloadUIAdapterKey[] = "";
}  // namespace

namespace offline_pages {

namespace {

bool RequestsMatchesGuid(const std::string& guid,
                         const SavePageRequest& request) {
  return request.client_id().id == guid &&
         GetPolicy(request.client_id().name_space).is_supported_by_download;
}

std::vector<int64_t> FilterRequestsByGuid(
    std::vector<std::unique_ptr<SavePageRequest>> requests,
    const std::string& guid) {
  std::vector<int64_t> request_ids;
  for (const auto& request : requests) {
    if (RequestsMatchesGuid(guid, *request))
      request_ids.push_back(request->request_id());
  }
  return request_ids;
}

}  // namespace

// static
DownloadUIAdapter* DownloadUIAdapter::FromOfflinePageModel(
    OfflinePageModel* model) {
  DCHECK(model);
  return static_cast<DownloadUIAdapter*>(
      model->GetUserData(kDownloadUIAdapterKey));
}

// static
void DownloadUIAdapter::AttachToOfflinePageModel(
    std::unique_ptr<DownloadUIAdapter> adapter,
    OfflinePageModel* model) {
  DCHECK(adapter);
  DCHECK(model);
  model->SetUserData(kDownloadUIAdapterKey, std::move(adapter));
}

DownloadUIAdapter::DownloadUIAdapter(
    OfflineContentAggregator* aggregator,
    OfflinePageModel* model,
    RequestCoordinator* request_coordinator,
    std::unique_ptr<VisualsDecoder> visuals_decoder,
    std::unique_ptr<Delegate> delegate)
    : aggregator_(aggregator),
      model_(model),
      request_coordinator_(request_coordinator),
      visuals_decoder_(std::move(visuals_decoder)),
      delegate_(std::move(delegate)) {
  delegate_->SetUIAdapter(this);
  if (aggregator_)
    aggregator_->RegisterProvider(kOfflinePageNamespace, this);
  if (model_)
    model_->AddObserver(this);
  if (request_coordinator_)
    request_coordinator_->AddObserver(this);
}

DownloadUIAdapter::~DownloadUIAdapter() {
  if (aggregator_)
    aggregator_->UnregisterProvider(kOfflinePageNamespace);
}

void DownloadUIAdapter::AddObserver(
    OfflineContentProvider::Observer* observer) {
  DCHECK(observer);
  if (observers_.HasObserver(observer))
    return;
  observers_.AddObserver(observer);
}

void DownloadUIAdapter::RemoveObserver(
    OfflineContentProvider::Observer* observer) {
  DCHECK(observer);
  if (!observers_.HasObserver(observer))
    return;
  observers_.RemoveObserver(observer);
}

void DownloadUIAdapter::OfflinePageModelLoaded(OfflinePageModel* model) {
  // This signal is not used here.
}

// OfflinePageModel::Observer
void DownloadUIAdapter::OfflinePageAdded(OfflinePageModel* model,
                                         const OfflinePageItem& added_page) {
  DCHECK(model == model_);
  if (!delegate_->IsVisibleInUI(added_page.client_id))
    return;

  const bool is_suggested =
      GetPolicy(added_page.client_id.name_space).is_suggested;

  OfflineItem offline_item(
      OfflineItemConversions::CreateOfflineItem(added_page, is_suggested));

  // We assume the pages which are non-suggested and shown in Download Home UI
  // should be coming from requests, so their corresponding offline items should
  // have been added to the UI when their corresponding requests were created.
  // So OnItemUpdated is used for non-suggested pages.
  // Otherwise, for pages of suggested articles, they'll be added to the UI
  // since they're added to Offline Page database directly, so OnItemsAdded is
  // used.
  for (auto& observer : observers_) {
    if (!is_suggested)
      observer.OnItemUpdated(offline_item, base::nullopt);
    else
      observer.OnItemsAdded({offline_item});
  }
}

// OfflinePageModel::Observer
void DownloadUIAdapter::OfflinePageDeleted(const OfflinePageItem& item) {
  if (!delegate_->IsVisibleInUI(item.client_id))
    return;

  for (auto& observer : observers_) {
    observer.OnItemRemoved(ContentId(kOfflinePageNamespace, item.client_id.id));
  }
}

// OfflinePageModel::Observer
void DownloadUIAdapter::ThumbnailAdded(OfflinePageModel* model,
                                       const int64_t offline_id,
                                       const std::string& thumbnail) {
  model_->GetPageByOfflineId(
      offline_id, base::BindOnce(&DownloadUIAdapter::OnPageGetForThumbnailAdded,
                                 weak_ptr_factory_.GetWeakPtr()));
}

// RequestCoordinator::Observer
void DownloadUIAdapter::OnAdded(const SavePageRequest& added_request) {
  if (!delegate_->IsVisibleInUI(added_request.client_id()))
    return;

  OfflineItem offline_item(
      OfflineItemConversions::CreateOfflineItem(added_request));

  for (auto& observer : observers_)
    observer.OnItemsAdded({offline_item});
}

// RequestCoordinator::Observer
void DownloadUIAdapter::OnCompleted(
    const SavePageRequest& request,
    RequestNotifier::BackgroundSavePageResult status) {
  if (!delegate_->IsVisibleInUI(request.client_id()))
    return;

  if (delegate_->MaybeSuppressNotification(request.request_origin(),
                                           request.client_id())) {
    return;
  }

  OfflineItem item = OfflineItemConversions::CreateOfflineItem(request);
  if (status == RequestNotifier::BackgroundSavePageResult::SUCCESS) {
    // If the request is completed successfully, it means there should already
    // be a OfflinePageAdded fired. So doing nothing in this case.
  } else if (status ==
                 RequestNotifier::BackgroundSavePageResult::USER_CANCELED ||
             status == RequestNotifier::BackgroundSavePageResult::
                           DOWNLOAD_THROTTLED) {
    for (auto& observer : observers_)
      observer.OnItemRemoved(item.id);
  } else {
    item.state = offline_items_collection::OfflineItemState::FAILED;
    // Actual cause could be server or network related, but we need to pick
    // a fail_state.
    item.fail_state = offline_items_collection::FailState::SERVER_FAILED;
    for (auto& observer : observers_)
      observer.OnItemUpdated(item, base::nullopt);
  }
}

// RequestCoordinator::Observer
void DownloadUIAdapter::OnChanged(const SavePageRequest& request) {
  if (!delegate_->IsVisibleInUI(request.client_id()))
    return;

  OfflineItem offline_item(OfflineItemConversions::CreateOfflineItem(request));
  for (OfflineContentProvider::Observer& observer : observers_)
    observer.OnItemUpdated(offline_item, base::nullopt);
}

// RequestCoordinator::Observer
void DownloadUIAdapter::OnNetworkProgress(const SavePageRequest& request,
                                          int64_t received_bytes) {
  if (!delegate_->IsVisibleInUI(request.client_id()))
    return;

  OfflineItem offline_item(OfflineItemConversions::CreateOfflineItem(request));
  offline_item.received_bytes = received_bytes;
  for (auto& observer : observers_)
    observer.OnItemUpdated(offline_item, base::nullopt);
}

void DownloadUIAdapter::GetAllItems(
    OfflineContentProvider::MultipleItemCallback callback) {
  std::unique_ptr<OfflineContentProvider::OfflineItemList> offline_items =
      std::make_unique<OfflineContentProvider::OfflineItemList>();
  model_->GetAllPages(base::BindOnce(
      &DownloadUIAdapter::OnOfflinePagesLoaded, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), std::move(offline_items)));
}

void DownloadUIAdapter::GetVisualsForItem(const ContentId& id,
                                          GetVisualsOptions options,
                                          VisualsCallback visuals_callback) {
  PageCriteria criteria;
  criteria.guid = id.id;
  criteria.maximum_matches = 1;
  model_->GetPagesWithCriteria(
      criteria, base::BindOnce(&DownloadUIAdapter::OnPageGetForVisuals,
                               weak_ptr_factory_.GetWeakPtr(), id, options,
                               std::move(visuals_callback)));
}

void DownloadUIAdapter::GetShareInfoForItem(const ContentId& id,
                                            ShareCallback share_callback) {
  delegate_->GetShareInfoForItem(id, std::move(share_callback));
}

void DownloadUIAdapter::RenameItem(const ContentId& id,
                                   const std::string& name,
                                   RenameCallback callback) {
  NOTREACHED();
}

void DownloadUIAdapter::OnPageGetForVisuals(
    const ContentId& id,
    GetVisualsOptions options,
    VisualsCallback visuals_callback,
    const std::vector<OfflinePageItem>& pages) {
  if (pages.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(visuals_callback), id, nullptr));
    return;
  }
  const OfflinePageItem* page = &pages[0];
  VisualResultCallback callback =
      base::BindOnce(std::move(visuals_callback), id);
  if (page->client_id.name_space == kSuggestedArticlesNamespace) {
    // Report PrefetchedItemHasThumbnail along with result callback.
    auto report_and_callback =
        [](VisualResultCallback result_callback,
           std::unique_ptr<offline_items_collection::OfflineItemVisuals>
               visuals) {
          UMA_HISTOGRAM_BOOLEAN(
              "OfflinePages.DownloadUI.PrefetchedItemHasThumbnail",
              visuals && !visuals->icon.IsEmpty());
          std::move(result_callback).Run(std::move(visuals));
        };
    callback = base::BindOnce(report_and_callback, std::move(callback));
  }

  model_->GetVisualsByOfflineId(
      page->offline_id, base::BindOnce(&DownloadUIAdapter::OnVisualsLoaded,
                                       weak_ptr_factory_.GetWeakPtr(), options,
                                       std::move(callback)));
}

void DownloadUIAdapter::OnVisualsLoaded(
    GetVisualsOptions options,
    VisualResultCallback callback,
    std::unique_ptr<OfflinePageVisuals> visuals) {
  DCHECK(visuals_decoder_);
  if (!visuals || (visuals->thumbnail.empty() && visuals->favicon.empty())) {
    // PostTask not required, GetVisualsByOfflineId does it for us.
    std::move(callback).Run(nullptr);
    return;
  }

  DecodeThumbnail(std::move(visuals), options, std::move(callback));
}

void DownloadUIAdapter::DecodeThumbnail(
    std::unique_ptr<OfflinePageVisuals> visuals,
    GetVisualsOptions options,
    VisualResultCallback callback) {
  if (!options.get_icon) {
    DecodeFavicon(std::move(visuals->favicon), options, std::move(callback),
                  gfx::Image());
    return;
  }

  // If visuals->thumbnail is empty, DecodeAndCropImage will give the
  // callback an empty gfx::Image.
  visuals_decoder_->DecodeAndCropImage(
      visuals->thumbnail, base::BindOnce(&DownloadUIAdapter::DecodeFavicon,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         std::move(visuals->favicon), options,
                                         std::move(callback)));
}

void DownloadUIAdapter::DecodeFavicon(std::string favicon,
                                      GetVisualsOptions options,
                                      VisualResultCallback callback,
                                      const gfx::Image& thumbnail) {
  auto make_visuals_lambda = [](VisualResultCallback callback,
                                const gfx::Image& thumbnail,
                                const gfx::Image& favicon) {
    auto item_visuals =
        std::make_unique<offline_items_collection::OfflineItemVisuals>(
            thumbnail, favicon);
    std::move(callback).Run(std::move(item_visuals));
  };

  if (!options.get_custom_favicon) {
    make_visuals_lambda(std::move(callback), thumbnail, gfx::Image());
    return;
  }

  visuals_decoder_->DecodeAndCropImage(
      std::move(favicon),
      base::BindOnce(make_visuals_lambda, std::move(callback), thumbnail));
}

void DownloadUIAdapter::OnPageGetForThumbnailAdded(
    const OfflinePageItem* page) {
  if (!page)
    return;

  auto offline_item = OfflineItemConversions::CreateOfflineItem(
      *page, GetPolicy(page->client_id.name_space).is_suggested);

  offline_items_collection::UpdateDelta update_delta;
  update_delta.visuals_changed = true;
  for (auto& observer : observers_)
    observer.OnItemUpdated(offline_item, update_delta);
}

// TODO(dimich): Remove this method since it is not used currently. If needed,
// it has to be updated to fault in the initial load of items. Currently it
// simply returns nullopt if the cache is not loaded.
void DownloadUIAdapter::GetItemById(
    const ContentId& id,
    OfflineContentProvider::SingleItemCallback callback) {
  PageCriteria criteria;
  criteria.guid = id.id;
  criteria.maximum_matches = 1;
  model_->GetPagesWithCriteria(
      criteria,
      base::BindOnce(&DownloadUIAdapter::OnPageGetForGetItem,
                     weak_ptr_factory_.GetWeakPtr(), id, std::move(callback)));
}

void DownloadUIAdapter::OnPageGetForGetItem(
    const ContentId& id,
    OfflineContentProvider::SingleItemCallback callback,
    const std::vector<OfflinePageItem>& pages) {
  if (!pages.empty()) {
    const OfflinePageItem* page = &pages[0];
    const bool is_suggested =
        GetPolicy(page->client_id.name_space).is_suggested;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  OfflineItemConversions::CreateOfflineItem(
                                      *page, is_suggested)));
    return;
  }
  request_coordinator_->GetAllRequests(
      base::BindOnce(&DownloadUIAdapter::OnAllRequestsGetForGetItem,
                     weak_ptr_factory_.GetWeakPtr(), id, std::move(callback)));
}

void DownloadUIAdapter::OnAllRequestsGetForGetItem(
    const ContentId& id,
    OfflineContentProvider::SingleItemCallback callback,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  base::Optional<OfflineItem> offline_item;
  for (const auto& request : requests) {
    if (request->client_id().id == id.id)
      offline_item = OfflineItemConversions::CreateOfflineItem(*request);
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), offline_item));
}

void DownloadUIAdapter::OpenItem(LaunchLocation location, const ContentId& id) {
  PageCriteria criteria;
  criteria.guid = id.id;
  criteria.maximum_matches = 1;
  model_->GetPagesWithCriteria(
      criteria, base::BindOnce(&DownloadUIAdapter::OnPageGetForOpenItem,
                               weak_ptr_factory_.GetWeakPtr(), location));
}

void DownloadUIAdapter::OnPageGetForOpenItem(
    LaunchLocation location,
    const std::vector<OfflinePageItem>& pages) {
  if (pages.empty())
    return;
  const OfflinePageItem* page = &pages[0];
  const bool is_suggested = GetPolicy(page->client_id.name_space).is_suggested;
  OfflineItem item =
      OfflineItemConversions::CreateOfflineItem(*page, is_suggested);
  delegate_->OpenItem(item, page->offline_id, location);
}

void DownloadUIAdapter::RemoveItem(const ContentId& id) {
  PageCriteria criteria;
  criteria.supported_by_downloads = true;
  criteria.guid = id.id;
  model_->DeletePagesWithCriteria(
      criteria, base::BindRepeating(&DownloadUIAdapter::OnDeletePagesDone,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void DownloadUIAdapter::CancelDownload(const ContentId& id) {
  auto predicate = base::BindRepeating(&RequestsMatchesGuid, id.id);
  request_coordinator_->RemoveRequestsIf(predicate, base::DoNothing());
}

void DownloadUIAdapter::PauseDownload(const ContentId& id) {
  // TODO(fgorski): Clean this up in a way where 2 round trips + GetAllRequests
  // is not necessary.
  request_coordinator_->GetAllRequests(
      base::BindOnce(&DownloadUIAdapter::PauseDownloadContinuation,
                     weak_ptr_factory_.GetWeakPtr(), id.id));
}

void DownloadUIAdapter::PauseDownloadContinuation(
    const std::string& guid,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  request_coordinator_->PauseRequests(
      FilterRequestsByGuid(std::move(requests), guid));
}

void DownloadUIAdapter::ResumeDownload(const ContentId& id,
                                       bool has_user_gesture) {
  // TODO(fgorski): Clean this up in a way where 2 round trips + GetAllRequests
  // is not necessary.
  if (has_user_gesture) {
    request_coordinator_->GetAllRequests(
        base::BindOnce(&DownloadUIAdapter::ResumeDownloadContinuation,
                       weak_ptr_factory_.GetWeakPtr(), id.id));
  } else {
    request_coordinator_->StartImmediateProcessing(base::DoNothing());
  }
}

void DownloadUIAdapter::ResumeDownloadContinuation(
    const std::string& guid,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  request_coordinator_->ResumeRequests(
      FilterRequestsByGuid(std::move(requests), guid));
}

void DownloadUIAdapter::OnOfflinePagesLoaded(
    OfflineContentProvider::MultipleItemCallback callback,
    std::unique_ptr<OfflineContentProvider::OfflineItemList> offline_items,
    const MultipleOfflinePageItemResult& pages) {
  for (const auto& page : pages) {
    if (delegate_->IsVisibleInUI(page.client_id)) {
      std::string guid = page.client_id.id;
      offline_items->push_back(OfflineItemConversions::CreateOfflineItem(
          page, GetPolicy(page.client_id.name_space).is_suggested));
    }
  }
  request_coordinator_->GetAllRequests(base::BindOnce(
      &DownloadUIAdapter::OnRequestsLoaded, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), std::move(offline_items)));
}

void DownloadUIAdapter::OnRequestsLoaded(
    OfflineContentProvider::MultipleItemCallback callback,
    std::unique_ptr<OfflineContentProvider::OfflineItemList> offline_items,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  for (const auto& request : requests) {
    if (delegate_->IsVisibleInUI(request->client_id())) {
      std::string guid = request->client_id().id;
      offline_items->push_back(
          OfflineItemConversions::CreateOfflineItem(*request.get()));
    }
  }

  OfflineContentProvider::OfflineItemList list = *offline_items;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), list));
}

void DownloadUIAdapter::OnDeletePagesDone(DeletePageResult result) {
  // TODO(dimich): Consider adding UMA to record user actions.
}

}  // namespace offline_pages

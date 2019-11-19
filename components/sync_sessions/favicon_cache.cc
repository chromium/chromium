// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/favicon_cache.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "components/favicon/core/favicon_service.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/time.h"
#include "components/sync/protocol/favicon_image_specifics.pb.h"
#include "components/sync/protocol/favicon_tracking_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "ui/gfx/favicon_size.h"

namespace sync_sessions {

// Synced favicon storage and tracking.
// Note: we don't use the favicon service for storing these because these
// favicons are not necessarily associated with any local navigation, and
// hence would not work with the current expiration logic. We have custom
// expiration logic based on visit time/bookmark status/etc.
// See crbug.com/122890.
struct SyncedFaviconInfo {
  explicit SyncedFaviconInfo(const GURL& favicon_url)
      : favicon_url(favicon_url),
        is_bookmarked(false),
        received_local_update(false) {}

  // The actual favicon data.
  // TODO(zea): don't keep around the actual data for locally sourced
  // favicons (UI can access those directly).
  favicon_base::FaviconRawBitmapResult bitmap_data[NUM_SIZES];
  // The URL this favicon was loaded from.
  const GURL favicon_url;
  // Is the favicon for a bookmarked page?
  bool is_bookmarked;
  // The last time a tab needed this favicon.
  // Note: Do not modify this directly! It should only be modified via
  // UpdateFaviconVisitTime(..).
  base::Time last_visit_time;
  // Whether we've received a local update for this favicon since starting up.
  bool received_local_update;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncedFaviconInfo);
};

// Information for handling local favicon updates. Used in
// OnFaviconDataAvailable.
struct LocalFaviconUpdateInfo {
  LocalFaviconUpdateInfo()
      : new_image(false),
        new_tracking(false),
        image_needs_rewrite(false),
        favicon_info(nullptr) {}

  bool new_image;
  bool new_tracking;
  bool image_needs_rewrite;
  SyncedFaviconInfo* favicon_info;
};

namespace {

// Maximum width/height resolution supported.
const int kMaxFaviconResolution = 16;

// Returns a mask of the supported favicon types.
// TODO(zea): Supporting other favicons types will involve some work in the
// favicon service and navigation controller. See crbug.com/181068.
favicon_base::IconTypeSet SupportedFaviconTypes() {
  return {favicon_base::IconType::kFavicon};
}

// Returns the appropriate IconSize to use for a given gfx::Size pixel
// dimensions.
IconSize GetIconSizeBinFromBitmapResult(const gfx::Size& pixel_size) {
  int max_size =
      (pixel_size.width() > pixel_size.height() ?
       pixel_size.width() : pixel_size.height());
  // TODO(zea): re-enable 64p and 32p resolutions once we support them.
  if (max_size > 64)
    return SIZE_INVALID;
  else if (max_size > 32)
    return SIZE_INVALID;
  else if (max_size > 16)
    return SIZE_INVALID;
  else
    return SIZE_16;
}

// Helper for debug statements.
std::string IconSizeToString(IconSize icon_size) {
  switch (icon_size) {
    case SIZE_16:
      return "16";
    case SIZE_32:
      return "32";
    case SIZE_64:
      return "64";
    default:
      return "INVALID";
  }
}

// Extract the favicon url from either of the favicon types.
GURL GetFaviconURLFromSpecifics(const sync_pb::EntitySpecifics& specifics) {
  if (specifics.has_favicon_tracking())
    return GURL(specifics.favicon_tracking().favicon_url());
  else
    return GURL(specifics.favicon_image().favicon_url());
}

// Convert protobuf image data into a FaviconRawBitmapResult.
favicon_base::FaviconRawBitmapResult GetImageDataFromSpecifics(
    const sync_pb::FaviconData& favicon_data) {
  base::RefCountedString* temp_string =
      new base::RefCountedString();
  temp_string->data() = favicon_data.favicon();
  favicon_base::FaviconRawBitmapResult bitmap_result;
  bitmap_result.bitmap_data = temp_string;
  bitmap_result.pixel_size.set_height(favicon_data.height());
  bitmap_result.pixel_size.set_width(favicon_data.width());
  return bitmap_result;
}

// Convert a FaviconRawBitmapResult into protobuf image data.
void FillSpecificsWithImageData(
    const favicon_base::FaviconRawBitmapResult& bitmap_result,
    sync_pb::FaviconData* favicon_data) {
  if (!bitmap_result.bitmap_data.get())
    return;
  favicon_data->set_height(bitmap_result.pixel_size.height());
  favicon_data->set_width(bitmap_result.pixel_size.width());
  favicon_data->set_favicon(bitmap_result.bitmap_data->front(),
                            bitmap_result.bitmap_data->size());
}

// Build a FaviconImageSpecifics from a SyncedFaviconInfo.
void BuildImageSpecifics(
    const SyncedFaviconInfo* favicon_info,
    sync_pb::FaviconImageSpecifics* image_specifics) {
  image_specifics->set_favicon_url(favicon_info->favicon_url.spec());
  FillSpecificsWithImageData(favicon_info->bitmap_data[SIZE_16],
                             image_specifics->mutable_favicon_web());
  // TODO(zea): bring this back if we can handle the load.
  // FillSpecificsWithImageData(favicon_info->bitmap_data[SIZE_32],
  //                            image_specifics->mutable_favicon_web_32());
  // FillSpecificsWithImageData(favicon_info->bitmap_data[SIZE_64],
  //                            image_specifics->mutable_favicon_touch_64());
}

// Build a FaviconTrackingSpecifics from a SyncedFaviconInfo.
void BuildTrackingSpecifics(
    const SyncedFaviconInfo* favicon_info,
    sync_pb::FaviconTrackingSpecifics* tracking_specifics) {
  tracking_specifics->set_favicon_url(favicon_info->favicon_url.spec());
  tracking_specifics->set_last_visit_time_ms(
      syncer::TimeToProtoTime(favicon_info->last_visit_time));
  tracking_specifics->set_is_bookmarked(favicon_info->is_bookmarked);
}

// Updates |favicon_info| with the image data in |bitmap_result|.
bool UpdateFaviconFromBitmapResult(
    const favicon_base::FaviconRawBitmapResult& bitmap_result,
    SyncedFaviconInfo* favicon_info) {
  DCHECK_EQ(favicon_info->favicon_url, bitmap_result.icon_url);
  if (!bitmap_result.is_valid()) {
    DVLOG(1) << "Received invalid favicon at " << bitmap_result.icon_url.spec();
    return false;
  }

  IconSize icon_size = GetIconSizeBinFromBitmapResult(
      bitmap_result.pixel_size);
  if (icon_size == SIZE_INVALID) {
    DVLOG(1) << "Ignoring unsupported resolution "
             << bitmap_result.pixel_size.height() << "x"
             << bitmap_result.pixel_size.width();
    return false;
  } else if (!favicon_info->bitmap_data[icon_size].bitmap_data.get() ||
             !favicon_info->received_local_update) {
    DVLOG(1) << "Storing " << IconSizeToString(icon_size) << "p"
             << " favicon for " << favicon_info->favicon_url.spec()
             << " with size " << bitmap_result.bitmap_data->size()
             << " bytes.";
    favicon_info->bitmap_data[icon_size] = bitmap_result;
    favicon_info->received_local_update = true;
    return true;
  } else {
    // We only allow updating the image data once per restart.
    DVLOG(2) << "Ignoring local update for " << bitmap_result.icon_url.spec();
    return false;
  }
}

bool FaviconInfoHasImages(const SyncedFaviconInfo& favicon_info) {
  return favicon_info.bitmap_data[SIZE_16].bitmap_data.get() ||
         favicon_info.bitmap_data[SIZE_32].bitmap_data.get() ||
         favicon_info.bitmap_data[SIZE_64].bitmap_data.get();
}

bool FaviconInfoHasTracking(const SyncedFaviconInfo& favicon_info) {
  return !favicon_info.last_visit_time.is_null();
}

bool FaviconInfoHasValidTypeData(const SyncedFaviconInfo& favicon_info,
                             syncer::ModelType type) {
  if (type == syncer::FAVICON_IMAGES)
    return FaviconInfoHasImages(favicon_info);
  else if (type == syncer::FAVICON_TRACKING)
    return FaviconInfoHasTracking(favicon_info);
  NOTREACHED();
  return false;
}

}  // namespace

FaviconCache::FaviconCache(favicon::FaviconService* favicon_service,
                           history::HistoryService* history_service,
                           int max_sync_favicon_limit)
    : favicon_service_(favicon_service),
      history_service_(history_service),
      max_sync_favicon_limit_(max_sync_favicon_limit) {
  if (history_service)
    history_service_observer_.Add(history_service);
  DVLOG(1) << "Setting favicon limit to " << max_sync_favicon_limit;
}

FaviconCache::~FaviconCache() {}

void FaviconCache::WaitUntilReadyToSync(base::OnceClosure done) {
  // |history_service_| can be null in tests. In that case, no point in waiting.
  if (!history_service_ || history_service_->backend_loaded()) {
    std::move(done).Run();
  } else {
    // Wait until HistoryService's backend loads, reported via
    // OnHistoryServiceLoaded().
    wait_until_ready_to_sync_cb_.push_back(std::move(done));
  }
}

syncer::SyncMergeResult FaviconCache::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
    std::unique_ptr<syncer::SyncErrorFactory> error_handler) {
  DCHECK(type == syncer::FAVICON_IMAGES || type == syncer::FAVICON_TRACKING);
  if (type == syncer::FAVICON_IMAGES)
    favicon_images_sync_processor_ = std::move(sync_processor);
  else
    favicon_tracking_sync_processor_ = std::move(sync_processor);

  syncer::SyncMergeResult merge_result(type);
  merge_result.set_num_items_before_association(synced_favicons_.size());
  std::set<GURL> unsynced_favicon_urls;
  for (const auto& url_icon_pair : synced_favicons_) {
    if (FaviconInfoHasValidTypeData(*url_icon_pair.second, type))
      unsynced_favicon_urls.insert(url_icon_pair.first);
  }

  syncer::SyncChangeList local_changes;
  for (auto iter = initial_sync_data.begin(); iter != initial_sync_data.end();
       ++iter) {
    GURL remote_url = GetFaviconURLFromSpecifics(iter->GetSpecifics());
    GURL favicon_url = GetLocalFaviconFromSyncedData(*iter);
    if (favicon_url.is_valid()) {
      unsynced_favicon_urls.erase(favicon_url);
      MergeSyncFavicon(*iter, &local_changes);
      merge_result.set_num_items_modified(
          merge_result.num_items_modified() + 1);
    } else {
      AddLocalFaviconFromSyncedData(*iter);
      merge_result.set_num_items_added(merge_result.num_items_added() + 1);
    }
  }

  // Rather than trigger a bunch of deletions when we set up sync, we drop
  // local favicons. Those pages that are currently open are likely to result in
  // loading new favicons/refreshing old favicons anyways, at which point
  // they'll be re-added and the appropriate synced favicons will be evicted.
  // TODO(zea): implement a smarter ordering of the which favicons to drop.
  int available_favicons = max_sync_favicon_limit_ - initial_sync_data.size();
  for (auto iter = unsynced_favicon_urls.begin();
       iter != unsynced_favicon_urls.end(); ++iter) {
    if (available_favicons > 0) {
      local_changes.push_back(
          syncer::SyncChange(FROM_HERE,
                             syncer::SyncChange::ACTION_ADD,
                             CreateSyncDataFromLocalFavicon(type, *iter)));
      available_favicons--;
    } else {
      auto favicon_iter = synced_favicons_.find(*iter);
      DVLOG(1) << "Dropping local favicon "
               << favicon_iter->second->favicon_url.spec();
      DropPartialFavicon(favicon_iter, type);
      merge_result.set_num_items_deleted(merge_result.num_items_deleted() + 1);
    }
  }
  merge_result.set_num_items_after_association(synced_favicons_.size());

  if (type == syncer::FAVICON_IMAGES) {
    favicon_images_sync_processor_->ProcessSyncChanges(FROM_HERE,
                                                       local_changes);
  } else {
    favicon_tracking_sync_processor_->ProcessSyncChanges(FROM_HERE,
                                                         local_changes);
  }
  return merge_result;
}

void FaviconCache::StopSyncing(syncer::ModelType type) {
  favicon_images_sync_processor_.reset();
  favicon_tracking_sync_processor_.reset();
  cancelable_task_tracker_.TryCancelAll();
  page_task_map_.clear();
}

syncer::SyncDataList FaviconCache::GetAllSyncData(syncer::ModelType type)
    const {
  syncer::SyncDataList data_list;
  for (auto iter = synced_favicons_.begin(); iter != synced_favicons_.end();
       ++iter) {
    if ((type == syncer::FAVICON_IMAGES &&
         FaviconInfoHasImages(*iter->second)) ||
        (type == syncer::FAVICON_TRACKING &&
         FaviconInfoHasTracking(*iter->second))) {
      data_list.push_back(CreateSyncDataFromLocalFavicon(type, iter->first));
    }
  }
  return data_list;
}

syncer::SyncError FaviconCache::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  if (!favicon_images_sync_processor_.get() ||
      !favicon_tracking_sync_processor_.get()) {
    return syncer::SyncError(FROM_HERE,
                             syncer::SyncError::DATATYPE_ERROR,
                             "One or both favicon types disabled.",
                             change_list[0].sync_data().GetDataType());
  }

  syncer::SyncChangeList new_changes;
  syncer::SyncError error;
  syncer::ModelType type = syncer::UNSPECIFIED;
  for (auto iter = change_list.begin(); iter != change_list.end(); ++iter) {
    type = iter->sync_data().GetDataType();
    DCHECK(type == syncer::FAVICON_IMAGES || type == syncer::FAVICON_TRACKING);
    GURL favicon_url =
        GetFaviconURLFromSpecifics(iter->sync_data().GetSpecifics());
    if (!favicon_url.is_valid()) {
      error.Reset(FROM_HERE, "Received invalid favicon url.", type);
      break;
    }
    auto favicon_iter = synced_favicons_.find(favicon_url);
    if (iter->change_type() == syncer::SyncChange::ACTION_DELETE) {
      if (favicon_iter == synced_favicons_.end()) {
        // Two clients might wind up deleting different parts of the same
        // favicon, so ignore this.
        continue;
      } else {
        DVLOG(1) << "Deleting favicon at " << favicon_url.spec();
        // If we only have partial data for the favicon (which implies orphaned
        // nodes), delete the local favicon only if the type corresponds to the
        // partial data we have. If we do have orphaned nodes, we rely on the
        // expiration logic to remove them eventually.
        DropPartialFavicon(favicon_iter, type);
      }
    } else if (iter->change_type() == syncer::SyncChange::ACTION_UPDATE ||
               iter->change_type() == syncer::SyncChange::ACTION_ADD) {
      // Adds and updates are treated the same due to the lack of strong
      // consistency (it's possible we'll receive an update for a tracking info
      // before we've received the add for the image, and should handle both
      // gracefully).
      if (favicon_iter == synced_favicons_.end()) {
        DVLOG(1) << "Adding favicon at " << favicon_url.spec();
        AddLocalFaviconFromSyncedData(iter->sync_data());
      } else {
        DVLOG(1) << "Updating favicon at " << favicon_url.spec();
        MergeSyncFavicon(iter->sync_data(), &new_changes);
      }
    } else {
      error.Reset(FROM_HERE, "Invalid action received.", type);
      break;
    }
  }

  // Note: we deliberately do not expire favicons here. If we received new
  // favicons and are now over the limit, the next local favicon change will
  // trigger the necessary expiration.
  if (!error.IsSet() && !new_changes.empty()) {
    if (type == syncer::FAVICON_IMAGES) {
        favicon_images_sync_processor_->ProcessSyncChanges(FROM_HERE,
                                                           new_changes);
    } else {
        favicon_tracking_sync_processor_->ProcessSyncChanges(FROM_HERE,
                                                             new_changes);
    }
  }

  return error;
}

void FaviconCache::OnPageFaviconUpdated(const GURL& page_url,
                                        base::Time mtime) {
  DCHECK(page_url.is_valid());

  // If a favicon load is already happening for this url, let it finish.
  if (page_task_map_.find(page_url) != page_task_map_.end())
    return;

  auto url_iter = page_favicon_map_.find(page_url);
  if (url_iter != page_favicon_map_.end()) {
    auto icon_iter = synced_favicons_.find(url_iter->second);
    // TODO(zea): consider what to do when only a subset of supported
    // resolutions are available.
    if (icon_iter != synced_favicons_.end() &&
        icon_iter->second->bitmap_data[SIZE_16].bitmap_data.get()) {
      DVLOG(2) << "Using cached favicon url for " << page_url.spec()
               << ": " << icon_iter->second->favicon_url.spec();
      UpdateFaviconVisitTime(icon_iter->second->favicon_url, mtime);
      UpdateSyncState(icon_iter->second->favicon_url,
                      syncer::SyncChange::ACTION_INVALID,
                      syncer::SyncChange::ACTION_UPDATE);
      return;
    }
  }

  DVLOG(1) << "Triggering favicon load for url " << page_url.spec();

  if (!favicon_service_) {
    page_task_map_[page_url] = 0;  // For testing only.
    return;
  }

  // TODO(zea): This appears to only fetch one favicon (best match based on
  // desired_size_in_dip). Figure out a way to fetch all favicons we support.
  // See crbug.com/181068.
  base::CancelableTaskTracker::TaskId id =
      favicon_service_->GetFaviconForPageURL(
          page_url, SupportedFaviconTypes(), kMaxFaviconResolution,
          base::Bind(&FaviconCache::OnFaviconDataAvailable,
                     weak_ptr_factory_.GetWeakPtr(), page_url, mtime),
          &cancelable_task_tracker_);
  page_task_map_[page_url] = id;
}

void FaviconCache::OnFaviconVisited(const GURL& page_url,
                                    const GURL& favicon_url) {
  DCHECK(page_url.is_valid());
  if (!favicon_url.is_valid() ||
      synced_favicons_.find(favicon_url) == synced_favicons_.end()) {
    // TODO(zea): consider triggering a favicon load if we have some but not
    // all desired resolutions?
    OnPageFaviconUpdated(page_url, base::Time::Now());
    return;
  }

  DVLOG(1) << "Associating " << page_url.spec() << " with favicon at "
           << favicon_url.spec() << " and marking visited.";
  page_favicon_map_[page_url] = favicon_url;
  bool had_tracking = FaviconInfoHasTracking(
      *synced_favicons_.find(favicon_url)->second);
  UpdateFaviconVisitTime(favicon_url, base::Time::Now());

  UpdateSyncState(favicon_url,
                  syncer::SyncChange::ACTION_INVALID,
                  (had_tracking ?
                   syncer::SyncChange::ACTION_UPDATE :
                   syncer::SyncChange::ACTION_ADD));
}

favicon_base::FaviconRawBitmapResult
FaviconCache::GetSyncedFaviconForFaviconURL(const GURL& favicon_url) const {
  if (!favicon_url.is_valid())
    return favicon_base::FaviconRawBitmapResult();
  auto iter = synced_favicons_.find(favicon_url);

  UMA_HISTOGRAM_BOOLEAN("Sync.FaviconCacheLookupSucceeded",
                        iter != synced_favicons_.end());
  if (iter == synced_favicons_.end())
    return favicon_base::FaviconRawBitmapResult();

  // TODO(zea): support getting other resolutions.
  if (!iter->second->bitmap_data[SIZE_16].bitmap_data.get())
    return favicon_base::FaviconRawBitmapResult();

  favicon_base::FaviconRawBitmapResult sync_bitmap_result;
  // Size is at most 16x16.
  sync_bitmap_result.pixel_size = gfx::Size(16, 16);
  sync_bitmap_result.icon_type = favicon_base::IconType::kFavicon;
  sync_bitmap_result.icon_url = favicon_url;
  sync_bitmap_result.bitmap_data =
      iter->second->bitmap_data[SIZE_16].bitmap_data;
  return sync_bitmap_result;
}

favicon_base::FaviconRawBitmapResult FaviconCache::GetSyncedFaviconForPageURL(
    const GURL& page_url) const {
  if (!page_url.is_valid())
    return favicon_base::FaviconRawBitmapResult();
  GURL icon_url = GetIconUrlForPageUrl(page_url);
  if (icon_url.is_empty())
    return favicon_base::FaviconRawBitmapResult();

  return GetSyncedFaviconForFaviconURL(icon_url);
}

GURL FaviconCache::GetIconUrlForPageUrl(const GURL& page_url) const {
  auto iter = page_favicon_map_.find(page_url);
  if (iter == page_favicon_map_.end())
    return GURL();
  return iter->second;
}

void FaviconCache::UpdateMappingsFromForeignTab(const sync_pb::SessionTab& tab,
                                                base::Time visit_time) {
  for (const sync_pb::TabNavigation& navigation : tab.navigation()) {
    const GURL page_url(navigation.virtual_url());
    const GURL icon_url(navigation.favicon_url());

    if (!icon_url.is_valid() || !page_url.is_valid() ||
        icon_url.SchemeIs("data")) {
      continue;
    }

    DVLOG(1) << "Associating " << page_url << " with favicon at " << icon_url;
    page_favicon_map_[page_url] = icon_url;

    if (synced_favicons_.count(icon_url) != 0)
      UpdateFaviconVisitTime(icon_url, visit_time);
  }
}

base::WeakPtr<FaviconCache> FaviconCache::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

size_t FaviconCache::NumFaviconsForTest() const {
  return synced_favicons_.size();
}

size_t FaviconCache::NumTasksForTest() const {
  return page_task_map_.size();
}

base::Time FaviconCache::GetLastVisitTimeForTest(
    const GURL& favicon_url) const {
  auto iter = synced_favicons_.find(favicon_url);
  DCHECK(iter != synced_favicons_.end());
  return iter->second->last_visit_time;
}

bool FaviconCache::FaviconRecencyFunctor::operator()(
    const SyncedFaviconInfo* lhs,
    const SyncedFaviconInfo* rhs) const {
  // TODO(zea): incorporate bookmarked status here once we care about it.
  if (lhs->last_visit_time < rhs->last_visit_time)
    return true;
  else if (lhs->last_visit_time == rhs->last_visit_time)
    return lhs->favicon_url.spec() < rhs->favicon_url.spec();
  return false;
}

void FaviconCache::OnFaviconDataAvailable(
    const GURL& page_url,
    base::Time mtime,
    const std::vector<favicon_base::FaviconRawBitmapResult>& bitmap_results) {
  auto page_iter = page_task_map_.find(page_url);
  if (page_iter == page_task_map_.end())
    return;
  page_task_map_.erase(page_iter);

  if (bitmap_results.size() == 0) {
    // Either the favicon isn't loaded yet or there is no valid favicon.
    // We already cleared the task id, so just return.
    DVLOG(1) << "Favicon load failed for page " << page_url.spec();
    return;
  }

  std::map<GURL, LocalFaviconUpdateInfo> favicon_updates;
  for (size_t i = 0; i < bitmap_results.size(); ++i) {
    const favicon_base::FaviconRawBitmapResult& bitmap_result =
        bitmap_results[i];
    GURL favicon_url = bitmap_result.icon_url;
    if (!favicon_url.is_valid() || favicon_url.SchemeIs("data"))
      continue;  // Can happen if the page is still loading.

    SyncedFaviconInfo* favicon_info = GetFaviconInfo(favicon_url);
    if (!favicon_info)
      return;  // We reached the in-memory limit.

    favicon_updates[favicon_url].new_image |=
        !FaviconInfoHasImages(*favicon_info);
    favicon_updates[favicon_url].new_tracking |=
        !FaviconInfoHasTracking(*favicon_info);
    favicon_updates[favicon_url].image_needs_rewrite |=
        UpdateFaviconFromBitmapResult(bitmap_result, favicon_info);
    favicon_updates[favicon_url].favicon_info = favicon_info;
  }

  for (std::map<GURL, LocalFaviconUpdateInfo>::const_iterator
           iter = favicon_updates.begin(); iter != favicon_updates.end();
       ++iter) {
    SyncedFaviconInfo* favicon_info = iter->second.favicon_info;
    const GURL& favicon_url = favicon_info->favicon_url;

    // TODO(zea): support multiple favicon urls per page.
    page_favicon_map_[page_url] = favicon_url;

    favicon_info->received_local_update = true;
    UpdateFaviconVisitTime(favicon_url, mtime);

    syncer::SyncChange::SyncChangeType image_change =
        syncer::SyncChange::ACTION_INVALID;
    if (iter->second.new_image)
      image_change = syncer::SyncChange::ACTION_ADD;
    else if (iter->second.image_needs_rewrite)
      image_change = syncer::SyncChange::ACTION_UPDATE;
    syncer::SyncChange::SyncChangeType tracking_change =
        syncer::SyncChange::ACTION_UPDATE;
    if (iter->second.new_tracking)
      tracking_change = syncer::SyncChange::ACTION_ADD;
    UpdateSyncState(favicon_url, image_change, tracking_change);
  }
}

void FaviconCache::UpdateSyncState(
    const GURL& icon_url,
    syncer::SyncChange::SyncChangeType image_change_type,
    syncer::SyncChange::SyncChangeType tracking_change_type) {
  DCHECK(icon_url.is_valid());
  // It's possible that we'll receive a favicon update before both types
  // have finished setting up. In that case ignore the update.
  // TODO(zea): consider tracking these skipped updates somehow?
  if (!favicon_images_sync_processor_.get() ||
      !favicon_tracking_sync_processor_.get()) {
    return;
  }

  auto iter = synced_favicons_.find(icon_url);
  DCHECK(iter != synced_favicons_.end());
  const SyncedFaviconInfo* favicon_info = iter->second.get();

  syncer::SyncChangeList image_changes;
  syncer::SyncChangeList tracking_changes;
  if (image_change_type != syncer::SyncChange::ACTION_INVALID) {
    sync_pb::EntitySpecifics new_specifics;
    sync_pb::FaviconImageSpecifics* image_specifics =
        new_specifics.mutable_favicon_image();
    BuildImageSpecifics(favicon_info, image_specifics);

    image_changes.push_back(
        syncer::SyncChange(FROM_HERE,
                           image_change_type,
                           syncer::SyncData::CreateLocalData(
                               icon_url.spec(),
                               icon_url.spec(),
                               new_specifics)));
  }
  if (tracking_change_type != syncer::SyncChange::ACTION_INVALID) {
    sync_pb::EntitySpecifics new_specifics;
    sync_pb::FaviconTrackingSpecifics* tracking_specifics =
        new_specifics.mutable_favicon_tracking();
    BuildTrackingSpecifics(favicon_info, tracking_specifics);

    tracking_changes.push_back(
        syncer::SyncChange(FROM_HERE,
                           tracking_change_type,
                           syncer::SyncData::CreateLocalData(
                               icon_url.spec(),
                               icon_url.spec(),
                               new_specifics)));
  }
  ExpireFaviconsIfNecessary(&image_changes, &tracking_changes);
  if (!image_changes.empty()) {
    favicon_images_sync_processor_->ProcessSyncChanges(FROM_HERE,
                                                       image_changes);
  }
  if (!tracking_changes.empty()) {
    favicon_tracking_sync_processor_->ProcessSyncChanges(FROM_HERE,
                                                         tracking_changes);
  }
}

SyncedFaviconInfo* FaviconCache::GetFaviconInfo(
    const GURL& icon_url) {
  DCHECK_EQ(recent_favicons_.size(), synced_favicons_.size());
  if (synced_favicons_.count(icon_url) != 0)
    return synced_favicons_[icon_url].get();

  // TODO(zea): implement in-memory eviction.
  DVLOG(1) << "Adding favicon info for " << icon_url.spec();
  auto favicon_info = std::make_unique<SyncedFaviconInfo>(icon_url);
  SyncedFaviconInfo* favicon_info_ptr = favicon_info.get();
  synced_favicons_[icon_url] = std::move(favicon_info);
  recent_favicons_.insert(favicon_info_ptr);
  DCHECK_EQ(recent_favicons_.size(), synced_favicons_.size());
  return favicon_info_ptr;
}

void FaviconCache::UpdateFaviconVisitTime(const GURL& icon_url,
                                          base::Time time) {
  DCHECK_EQ(recent_favicons_.size(), synced_favicons_.size());
  auto iter = synced_favicons_.find(icon_url);
  DCHECK(iter != synced_favicons_.end());
  if (iter->second->last_visit_time >= time)
    return;
  // Erase, update the time, then re-insert to maintain ordering.
  recent_favicons_.erase(iter->second.get());
  DVLOG(1) << "Updating " << icon_url.spec() << " visit time to "
           << syncer::GetTimeDebugString(time);
  iter->second->last_visit_time = time;
  recent_favicons_.insert(iter->second.get());

  if (VLOG_IS_ON(2)) {
    for (const auto* icon : recent_favicons_) {
      DVLOG(2) << "Favicon " << icon->favicon_url.spec() << ": "
               << syncer::GetTimeDebugString(icon->last_visit_time);
    }
  }
  DCHECK_EQ(recent_favicons_.size(), synced_favicons_.size());
}

void FaviconCache::ExpireFaviconsIfNecessary(
    syncer::SyncChangeList* image_changes,
    syncer::SyncChangeList* tracking_changes) {
  DCHECK_EQ(recent_favicons_.size(), synced_favicons_.size());
  // TODO(zea): once we have in-memory eviction, we'll need to track sync
  // favicon count separately from the synced_favicons_/recent_favicons_.

  // Iterate until we've removed the necessary amount. |recent_favicons_| is
  // already in recency order, so just start from the beginning.
  // TODO(zea): to reduce thrashing, consider removing more than the minimum.
  while (recent_favicons_.size() > max_sync_favicon_limit_) {
    SyncedFaviconInfo* candidate = *recent_favicons_.begin();
    DVLOG(1) << "Expiring favicon " << candidate->favicon_url.spec();
    DeleteSyncedFavicon(synced_favicons_.find(candidate->favicon_url),
                        image_changes,
                        tracking_changes);
  }
  DCHECK_EQ(recent_favicons_.size(), synced_favicons_.size());
}

GURL FaviconCache::GetLocalFaviconFromSyncedData(
    const syncer::SyncData& sync_favicon) const {
  syncer::ModelType type = sync_favicon.GetDataType();
  DCHECK(type == syncer::FAVICON_IMAGES || type == syncer::FAVICON_TRACKING);
  GURL favicon_url = GetFaviconURLFromSpecifics(sync_favicon.GetSpecifics());
  return (synced_favicons_.count(favicon_url) > 0 ? favicon_url : GURL());
}

void FaviconCache::MergeSyncFavicon(const syncer::SyncData& sync_favicon,
                                    syncer::SyncChangeList* sync_changes) {
  syncer::ModelType type = sync_favicon.GetDataType();
  DCHECK(type == syncer::FAVICON_IMAGES || type == syncer::FAVICON_TRACKING);
  sync_pb::EntitySpecifics new_specifics;
  GURL favicon_url = GetFaviconURLFromSpecifics(sync_favicon.GetSpecifics());
  auto iter = synced_favicons_.find(favicon_url);
  DCHECK(iter != synced_favicons_.end());
  SyncedFaviconInfo* favicon_info = iter->second.get();
  if (type == syncer::FAVICON_IMAGES) {
    sync_pb::FaviconImageSpecifics image_specifics =
        sync_favicon.GetSpecifics().favicon_image();

    // Remote image data always clobbers local image data.
    bool needs_update = false;
    if (image_specifics.has_favicon_web()) {
      favicon_info->bitmap_data[SIZE_16] = GetImageDataFromSpecifics(
          image_specifics.favicon_web());
    } else if (favicon_info->bitmap_data[SIZE_16].bitmap_data.get()) {
      needs_update = true;
    }
    if (image_specifics.has_favicon_web_32()) {
      favicon_info->bitmap_data[SIZE_32] = GetImageDataFromSpecifics(
          image_specifics.favicon_web_32());
    } else if (favicon_info->bitmap_data[SIZE_32].bitmap_data.get()) {
      needs_update = true;
    }
    if (image_specifics.has_favicon_touch_64()) {
      favicon_info->bitmap_data[SIZE_64] = GetImageDataFromSpecifics(
          image_specifics.favicon_touch_64());
    } else if (favicon_info->bitmap_data[SIZE_64].bitmap_data.get()) {
      needs_update = true;
    }

    if (needs_update)
      BuildImageSpecifics(favicon_info, new_specifics.mutable_favicon_image());
  } else {
    sync_pb::FaviconTrackingSpecifics tracking_specifics =
        sync_favicon.GetSpecifics().favicon_tracking();

    // Tracking data is merged, such that bookmark data is the logical OR
    // of the two, and last visit time is the most recent.

    base::Time last_visit =  syncer::ProtoTimeToTime(
        tracking_specifics.last_visit_time_ms());
    // Due to crbug.com/258196, there are tracking nodes out there with
    // null visit times. If this is one of those, artificially make it a valid
    // visit time, so we know the node exists and update it properly on the next
    // real visit.
    if (last_visit.is_null())
      last_visit = last_visit + base::TimeDelta::FromMilliseconds(1);
    UpdateFaviconVisitTime(favicon_url, last_visit);
    favicon_info->is_bookmarked = (favicon_info->is_bookmarked ||
                                   tracking_specifics.is_bookmarked());

    if (syncer::TimeToProtoTime(favicon_info->last_visit_time) !=
            tracking_specifics.last_visit_time_ms() ||
        favicon_info->is_bookmarked != tracking_specifics.is_bookmarked()) {
      BuildTrackingSpecifics(favicon_info,
                             new_specifics.mutable_favicon_tracking());
    }
    DCHECK(!favicon_info->last_visit_time.is_null());
  }

  if (new_specifics.has_favicon_image() ||
      new_specifics.has_favicon_tracking()) {
    sync_changes->push_back(syncer::SyncChange(
        FROM_HERE,
        syncer::SyncChange::ACTION_UPDATE,
        syncer::SyncData::CreateLocalData(favicon_url.spec(),
                                          favicon_url.spec(),
                                          new_specifics)));
  }
}

void FaviconCache::AddLocalFaviconFromSyncedData(
    const syncer::SyncData& sync_favicon) {
  syncer::ModelType type = sync_favicon.GetDataType();
  DCHECK(type == syncer::FAVICON_IMAGES || type == syncer::FAVICON_TRACKING);
  if (type == syncer::FAVICON_IMAGES) {
    sync_pb::FaviconImageSpecifics image_specifics =
        sync_favicon.GetSpecifics().favicon_image();
    GURL favicon_url = GURL(image_specifics.favicon_url());
    DCHECK(favicon_url.is_valid());
    DCHECK(!synced_favicons_.count(favicon_url));

    SyncedFaviconInfo* favicon_info = GetFaviconInfo(favicon_url);
    if (!favicon_info)
      return;  // We reached the in-memory limit.
    if (image_specifics.has_favicon_web()) {
      favicon_info->bitmap_data[SIZE_16] = GetImageDataFromSpecifics(
          image_specifics.favicon_web());
    }
    if (image_specifics.has_favicon_web_32()) {
      favicon_info->bitmap_data[SIZE_32] = GetImageDataFromSpecifics(
          image_specifics.favicon_web_32());
    }
    if (image_specifics.has_favicon_touch_64()) {
      favicon_info->bitmap_data[SIZE_64] = GetImageDataFromSpecifics(
          image_specifics.favicon_touch_64());
    }
  } else {
    sync_pb::FaviconTrackingSpecifics tracking_specifics =
        sync_favicon.GetSpecifics().favicon_tracking();
    GURL favicon_url = GURL(tracking_specifics.favicon_url());
    DCHECK(favicon_url.is_valid());
    DCHECK(!synced_favicons_.count(favicon_url));

    SyncedFaviconInfo* favicon_info = GetFaviconInfo(favicon_url);
    if (!favicon_info)
      return;  // We reached the in-memory limit.
    base::Time last_visit =  syncer::ProtoTimeToTime(
        tracking_specifics.last_visit_time_ms());
    // Due to crbug.com/258196, there are tracking nodes out there with
    // null visit times. If this is one of those, artificially make it a valid
    // visit time, so we know the node exists and update it properly on the next
    // real visit.
    if (last_visit.is_null())
      last_visit = last_visit + base::TimeDelta::FromMilliseconds(1);
    UpdateFaviconVisitTime(favicon_url, last_visit);
    favicon_info->is_bookmarked = tracking_specifics.is_bookmarked();
    DCHECK(!favicon_info->last_visit_time.is_null());
  }
}

syncer::SyncData FaviconCache::CreateSyncDataFromLocalFavicon(
    syncer::ModelType type,
    const GURL& favicon_url) const {
  DCHECK(type == syncer::FAVICON_IMAGES || type == syncer::FAVICON_TRACKING);
  DCHECK(favicon_url.is_valid());
  auto iter = synced_favicons_.find(favicon_url);
  DCHECK(iter != synced_favicons_.end());
  SyncedFaviconInfo* favicon_info = iter->second.get();

  syncer::SyncData data;
  sync_pb::EntitySpecifics specifics;
  if (type == syncer::FAVICON_IMAGES) {
    sync_pb::FaviconImageSpecifics* image_specifics =
        specifics.mutable_favicon_image();
    BuildImageSpecifics(favicon_info, image_specifics);
  } else {
    sync_pb::FaviconTrackingSpecifics* tracking_specifics =
        specifics.mutable_favicon_tracking();
    BuildTrackingSpecifics(favicon_info, tracking_specifics);
  }
  data = syncer::SyncData::CreateLocalData(favicon_url.spec(),
                                           favicon_url.spec(),
                                           specifics);
  return data;
}

void FaviconCache::DeleteSyncedFavicons(const std::set<GURL>& favicon_urls) {
  syncer::SyncChangeList image_deletions, tracking_deletions;
  for (auto iter = favicon_urls.begin(); iter != favicon_urls.end(); ++iter) {
    auto favicon_iter = synced_favicons_.find(*iter);
    if (favicon_iter == synced_favicons_.end())
      continue;
    DeleteSyncedFavicon(favicon_iter,
                        &image_deletions,
                        &tracking_deletions);
  }
  DVLOG(1) << "Deleting " << image_deletions.size() << " synced favicons.";
  if (favicon_images_sync_processor_.get()) {
    favicon_images_sync_processor_->ProcessSyncChanges(FROM_HERE,
                                                       image_deletions);
  }
  if (favicon_tracking_sync_processor_.get()) {
    favicon_tracking_sync_processor_->ProcessSyncChanges(FROM_HERE,
                                                         tracking_deletions);
  }
}

void FaviconCache::DeleteSyncedFavicon(
    FaviconMap::iterator favicon_iter,
    syncer::SyncChangeList* image_changes,
    syncer::SyncChangeList* tracking_changes) {
  SyncedFaviconInfo* favicon_info = favicon_iter->second.get();
  if (FaviconInfoHasImages(*(favicon_iter->second))) {
    DVLOG(1) << "Deleting image for "
             << favicon_iter->second.get()->favicon_url;
    image_changes->push_back(
        syncer::SyncChange(FROM_HERE,
                           syncer::SyncChange::ACTION_DELETE,
                           syncer::SyncData::CreateLocalDelete(
                               favicon_info->favicon_url.spec(),
                               syncer::FAVICON_IMAGES)));
  }
  if (FaviconInfoHasTracking(*(favicon_iter->second))) {
    DVLOG(1) << "Deleting tracking for "
             << favicon_iter->second.get()->favicon_url;
    tracking_changes->push_back(
        syncer::SyncChange(FROM_HERE,
                           syncer::SyncChange::ACTION_DELETE,
                           syncer::SyncData::CreateLocalDelete(
                               favicon_info->favicon_url.spec(),
                               syncer::FAVICON_TRACKING)));
  }
  DropSyncedFavicon(favicon_iter);
}

void FaviconCache::DropSyncedFavicon(FaviconMap::iterator favicon_iter) {
  DVLOG(1) << "Dropping favicon " << favicon_iter->second.get()->favicon_url;
  const GURL& url = favicon_iter->first;
  recent_favicons_.erase(favicon_iter->second.get());
  base::EraseIf(page_favicon_map_,
                [url](const auto& kv) { return kv.second == url; });
  synced_favicons_.erase(favicon_iter);
}

void FaviconCache::DropPartialFavicon(FaviconMap::iterator favicon_iter,
                                      syncer::ModelType type) {
  // If the type being dropped has no valid data, do nothing.
  if ((type == syncer::FAVICON_TRACKING &&
       !FaviconInfoHasTracking(*favicon_iter->second)) ||
      (type == syncer::FAVICON_IMAGES &&
       !FaviconInfoHasImages(*favicon_iter->second))) {
    return;
  }

  // If the type being dropped is the only type with valid data, just delete
  // the favicon altogether.
  if ((type == syncer::FAVICON_TRACKING &&
       !FaviconInfoHasImages(*favicon_iter->second)) ||
      (type == syncer::FAVICON_IMAGES &&
       !FaviconInfoHasTracking(*favicon_iter->second))) {
    DropSyncedFavicon(favicon_iter);
    return;
  }

  if (type == syncer::FAVICON_IMAGES) {
    DVLOG(1) << "Dropping favicon image "
             << favicon_iter->second.get()->favicon_url;
    for (int i = 0; i < NUM_SIZES; ++i) {
      favicon_iter->second->bitmap_data[i] =
          favicon_base::FaviconRawBitmapResult();
    }
    DCHECK(!FaviconInfoHasImages(*favicon_iter->second));
  } else {
    DCHECK_EQ(type, syncer::FAVICON_TRACKING);
    DVLOG(1) << "Dropping favicon tracking "
             << favicon_iter->second.get()->favicon_url;
    recent_favicons_.erase(favicon_iter->second.get());
    favicon_iter->second->last_visit_time = base::Time();
    favicon_iter->second->is_bookmarked = false;
    recent_favicons_.insert(favicon_iter->second.get());
    DCHECK(!FaviconInfoHasTracking(*favicon_iter->second));
  }
}

void FaviconCache::OnURLsDeleted(history::HistoryService* history_service,
                                 const history::DeletionInfo& deletion_info) {
  // We only care about actual user (or sync) deletions.
  if (deletion_info.is_from_expiration())
    return;

  if (!deletion_info.IsAllHistory()) {
    DeleteSyncedFavicons(deletion_info.favicon_urls());
    return;
  }

  // All history was cleared: just delete all favicons.
  DVLOG(1) << "History clear detected, deleting all synced favicons.";
  syncer::SyncChangeList image_deletions, tracking_deletions;
  while (!synced_favicons_.empty()) {
    DeleteSyncedFavicon(synced_favicons_.begin(), &image_deletions,
                        &tracking_deletions);
  }

  if (favicon_images_sync_processor_.get()) {
    favicon_images_sync_processor_->ProcessSyncChanges(FROM_HERE,
                                                       image_deletions);
  }
  if (favicon_tracking_sync_processor_.get()) {
    favicon_tracking_sync_processor_->ProcessSyncChanges(FROM_HERE,
                                                         tracking_deletions);
  }
}

void FaviconCache::OnHistoryServiceLoaded(
    history::HistoryService* history_service) {
  // Make a copy before iterating over, in case triggering the callback has side
  // effects.
  std::vector<base::OnceClosure> callbacks =
      std::move(wait_until_ready_to_sync_cb_);
  wait_until_ready_to_sync_cb_.clear();

  for (auto& cb : callbacks)
    std::move(cb).Run();
}

}  // namespace sync_sessions

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/downloads/offline_item_conversions.h"

#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/offline_items_collection/core/pending_state.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_item.h"

using OfflineItemFilter = offline_items_collection::OfflineItemFilter;
using OfflineItemState = offline_items_collection::OfflineItemState;
using OfflineItemProgressUnit =
    offline_items_collection::OfflineItemProgressUnit;
using PendingState = offline_items_collection::PendingState;

namespace {

const std::string GetDisplayName(const offline_pages::OfflinePageItem& page) {
  if (!page.title.empty())
    return base::UTF16ToUTF8(page.title);

  std::string host = page.url.host();
  return host.empty() ? page.url.spec() : host;
}

const std::string GetDisplayName(
    const offline_pages::SavePageRequest& request) {
  std::string host = request.url().host();
  return host.empty() ? request.url().spec() : host;
}

const std::string GetMimeType() {
  return "multipart/related";
}

}  // namespace

namespace offline_pages {

OfflineItem OfflineItemConversions::CreateOfflineItem(
    const OfflinePageItem& page) {
  OfflineItem item;
  item.id = ContentId(kOfflinePageNamespace, page.client_id.id);
  item.title = GetDisplayName(page);
  item.filter = OfflineItemFilter::FILTER_PAGE;
  item.state = OfflineItemState::COMPLETE;
  item.total_size_bytes = page.file_size;
  item.received_bytes = page.file_size;
  item.creation_time = page.creation_time;
  // Completion time is the time when the offline archive was created.
  item.completion_time = page.creation_time;
  item.last_accessed_time = page.last_access_time;
  item.file_path = page.file_path;
  item.mime_type = GetMimeType();
  item.url = page.url;
  item.original_url = page.original_url_if_different;
  item.progress.value = 100;
  item.progress.max = 100;
  item.progress.unit = OfflineItemProgressUnit::PERCENTAGE;
  item.is_openable = true;
  item.externally_removed = page.file_missing_time != base::Time();
  item.description = page.snippet;
  item.attribution = page.attribution;

  // TODO(carlosk): Set item.ignore_visuals here to the right thing.
  return item;
}

OfflineItem OfflineItemConversions::CreateOfflineItem(
    const SavePageRequest& request) {
  OfflineItem item;
  item.id = ContentId(kOfflinePageNamespace, request.client_id().id);
  item.title = GetDisplayName(request);
  item.filter = OfflineItemFilter::FILTER_PAGE;
  item.creation_time = request.creation_time();
  item.total_size_bytes = -1L;
  item.received_bytes = 0;
  item.mime_type = GetMimeType();
  item.url = request.url();
  item.original_url = request.original_url();
  switch (request.request_state()) {
    case SavePageRequest::RequestState::AVAILABLE:
      item.state = OfflineItemState::PENDING;
      item.pending_state = request.pending_state();
      break;
    case SavePageRequest::RequestState::OFFLINING:
      item.state = OfflineItemState::IN_PROGRESS;
      break;
    case SavePageRequest::RequestState::PAUSED:
      item.state = OfflineItemState::PAUSED;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  item.fail_state = request.fail_state();
  item.progress.value = 0;
  item.progress.unit = OfflineItemProgressUnit::PERCENTAGE;

  return item;
}

}  // namespace offline_pages

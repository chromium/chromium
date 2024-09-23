// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/request_coordinator_event_logger.h"

#include <string>
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"

namespace offline_pages {

namespace {

static std::string BackgroundSavePageResultToString(
    RequestNotifier::BackgroundSavePageResult result) {
  switch (result) {
    case RequestNotifier::BackgroundSavePageResult::SUCCESS:
      return "SUCCESS";
    case RequestNotifier::BackgroundSavePageResult::LOADING_FAILURE:
      return "LOADING_FAILURE";
    case RequestNotifier::BackgroundSavePageResult::LOADING_CANCELED:
      return "LOADING_CANCELED";
    case RequestNotifier::BackgroundSavePageResult::FOREGROUND_CANCELED:
      return "FOREGROUND_CANCELED";
    case RequestNotifier::BackgroundSavePageResult::SAVE_FAILED:
      return "SAVE_FAILED";
    case RequestNotifier::BackgroundSavePageResult::EXPIRED:
      return "EXPIRED";
    case RequestNotifier::BackgroundSavePageResult::RETRY_COUNT_EXCEEDED:
      return "RETRY_COUNT_EXCEEDED";
    case RequestNotifier::BackgroundSavePageResult::START_COUNT_EXCEEDED:
      return "START_COUNT_EXCEEDED";
    case RequestNotifier::BackgroundSavePageResult::USER_CANCELED:
      return "REMOVED";
    case RequestNotifier::BackgroundSavePageResult::DOWNLOAD_THROTTLED:
      return "DOWNLOAD_THROTTLED";
    default:
      NOTREACHED_IN_MIGRATION();
      return base::NumberToString(static_cast<int>(result));
  }
}

static std::string UpdateRequestResultToString(UpdateRequestResult result) {
  switch (result) {
    case UpdateRequestResult::SUCCESS:
      return "SUCCESS";
    case UpdateRequestResult::STORE_FAILURE:
      return "STORE_FAILURE";
    case UpdateRequestResult::REQUEST_DOES_NOT_EXIST:
      return "REQUEST_DOES_NOT_EXIST";
    default:
      NOTREACHED_IN_MIGRATION();
      return base::NumberToString(static_cast<int>(result));
  }
}

}  // namespace

void RequestCoordinatorEventLogger::RecordOfflinerResult(
    const std::string& name_space,
    Offliner::RequestStatus new_status,
    int64_t request_id) {
  std::string request_id_str = base::NumberToString(request_id);
  RecordActivity("Background save attempt for " + name_space + ":" +
                 request_id_str + " - " +
                 Offliner::RequestStatusToString(new_status));
}

void RequestCoordinatorEventLogger::RecordDroppedSavePageRequest(
    const std::string& name_space,
    RequestNotifier::BackgroundSavePageResult result,
    int64_t request_id) {
  std::string request_id_str = base::NumberToString(request_id);
  RecordActivity("Background save request removed " + name_space + ":" +
                 request_id_str + " - " +
                 BackgroundSavePageResultToString(result));
}

void RequestCoordinatorEventLogger::RecordUpdateRequestFailed(
    const std::string& name_space,
    UpdateRequestResult result) {
  RecordActivity("Updating queued request for " + name_space + " failed - " +
                 UpdateRequestResultToString(result));
}

void RequestCoordinatorEventLogger::RecordAddRequestFailed(
    const std::string& name_space,
    AddRequestResult result) {
  RecordActivity(
      base::StrCat({"Add request failed for ", name_space, " - code ",
                    base::NumberToString(static_cast<int>(result))}));
}

}  // namespace offline_pages

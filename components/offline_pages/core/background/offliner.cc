// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "components/offline_pages/core/background/offliner.h"

namespace offline_pages {

// static
std::string Offliner::RequestStatusToString(RequestStatus request_status) {
  switch (request_status) {
    case Offliner::RequestStatus::UNKNOWN:
      return "UNKNOWN";
    case Offliner::RequestStatus::LOADED:
      return "LOADED";
    case Offliner::RequestStatus::SAVED:
      return "SAVED";
    case Offliner::RequestStatus::REQUEST_COORDINATOR_CANCELED:
      return "REQUEST_COORDINATOR_CANCELED";
    case Offliner::RequestStatus::LOADING_CANCELED:
      return "LOADING_CANCELED";
    case Offliner::RequestStatus::LOADING_FAILED:
      return "LOADING_FAILED";
    case Offliner::RequestStatus::SAVE_FAILED:
      return "SAVE_FAILED";
    case Offliner::RequestStatus::FOREGROUND_CANCELED:
      return "FOREGROUND_CANCELED";
    case Offliner::RequestStatus::REQUEST_COORDINATOR_TIMED_OUT:
      return "REQUEST_COORDINATOR_TIMED_OUT";
    case Offliner::RequestStatus::DEPRECATED_LOADING_NOT_STARTED:
      NOTREACHED();
      return "DEPRECATED_LOADING_NOT_STARTED";
    case Offliner::RequestStatus::LOADING_FAILED_NO_RETRY:
      return "LOADING_FAILED_NO_RETRY";
    case Offliner::RequestStatus::LOADING_FAILED_NO_NEXT:
      return "LOADING_FAILED_NO_NEXT";
    case Offliner::RequestStatus::LOADING_NOT_ACCEPTED:
      return "LOADING_NOT_ACCEPTED";
    case Offliner::RequestStatus::QUEUE_UPDATE_FAILED:
      return "QUEUE_UPDATE_FAILED";
    case Offliner::RequestStatus::BACKGROUND_SCHEDULER_CANCELED:
      return "BACKGROUND_SCHEDULER_CANCELED";
    case Offliner::RequestStatus::SAVED_ON_LAST_RETRY:
      return "SAVED_ON_LAST_RETRY";
    case Offliner::RequestStatus::BROWSER_KILLED:
      return "BROWSER_KILLED";
    case Offliner::RequestStatus::LOADING_FAILED_DOWNLOAD:
      return "LOADING_FAILED_DOWNLOAD";
    case Offliner::RequestStatus::DOWNLOAD_THROTTLED:
      return "DOWNLOAD_THROTTLED";
    case Offliner::RequestStatus::LOADING_FAILED_NET_ERROR:
      return "LOADING_FAILED_NET_ERROR";
    case Offliner::RequestStatus::LOADING_FAILED_HTTP_ERROR:
      return "LOADING_FAILED_HTTP_ERROR";
    case Offliner::RequestStatus::LOADING_DEFERRED:
      return "LOADING_DEFERRED";
    case Offliner::RequestStatus::LOADED_PAGE_HAS_CERTIFICATE_ERROR:
      return "LOADED_PAGE_HAS_CERTIFICATE_ERROR";
    case Offliner::RequestStatus::LOADED_PAGE_IS_BLOCKED:
      return "LOADED_PAGE_IS_BLOCKED";
    case Offliner::RequestStatus::LOADED_PAGE_IS_CHROME_INTERNAL:
      return "LOADED_PAGE_IS_CHROME_INTERNAL";
  }
  return "UNKNOWN";
}

}  // namespace offline_pages

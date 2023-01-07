// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REQUEST_QUEUE_RESULTS_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REQUEST_QUEUE_RESULTS_H_

#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/offline_store_types.h"

namespace offline_pages {

// Extracted from RequestQueue so that we can build types that use these results
// that RequestQueue depends on (for example, the PickRequestTask).
typedef StoreUpdateResult<SavePageRequest> UpdateRequestsResult;

enum class GetRequestsResult {
  SUCCESS,
  STORE_FAILURE,
};

enum class AddRequestResult {
  SUCCESS,
  STORE_FAILURE,
  ALREADY_EXISTS,
  REQUEST_QUOTA_HIT,  // Cannot add a request with this namespace, as it has
                      // reached a quota of active requests.
  URL_ERROR,          // Cannot save this URL.
  DUPLICATE_URL,  // URL is already being requested from this name_space, and
                  // |disallow_duplicate_requests| was set to true.
};

// GENERATED_JAVA_ENUM_PACKAGE:org.chromium.components.offlinepages.background
enum class UpdateRequestResult {
  SUCCESS,
  STORE_FAILURE,
  REQUEST_DOES_NOT_EXIST,  // Failed to delete the request because it does not
                           // exist.
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REQUEST_QUEUE_RESULTS_H_

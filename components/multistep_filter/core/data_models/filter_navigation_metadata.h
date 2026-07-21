// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_FILTER_NAVIGATION_METADATA_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_FILTER_NAVIGATION_METADATA_H_

#include <optional>

#include "base/time/time.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace multistep_filter {

// Immutable snapshot of tab navigation state (network, security, and user
// gestures). Populated by the content layer and dispatched into the Core layer.
struct FilterNavigationMetadata {
  int64_t navigation_id = 0;
  base::TimeTicks navigation_start_time;
  base::TimeTicks navigation_finish_time;
  // The committed URL (at the end of the redirect chain).
  GURL url;
  // The URL of the last committed page in the primary main frame. This may be
  // empty if there was no last committed entry (e.g., in a newly opened tab).
  GURL prev_url;
  bool is_cryptographic_scheme = false;
  bool is_http_allowed_for_testing = false;
  // The net error code of the navigation. See net/base/net_errors.h.
  int net_error_code = 0;
  int http_response_code = 0;
  // Whether the navigation resulted in an error page (e.g., due to connection
  // failures, DNS errors) or returned a client/server HTTP error status code
  // (4xx or 5xx).
  bool is_error_page_navigation = false;
  // Whether the navigation was initiated by a user gesture (e.g., clicking a
  // link, submitting a form). This is also true for browser-initiated
  // navigations (e.g., typing in the omnibox, clicking a bookmark).
  bool has_user_gesture = false;
  // Whether the navigation was initiated by the user accepting a filter
  // suggestion cue.
  bool was_filter_initiated_navigation = false;
  // Whether the navigation happened without changing the document. Examples of
  // same-document navigations include reference fragment navigations (e.g.
  // #hash changes) and History API modifications (e.g. pushState/replaceState).
  bool is_same_document_navigation = false;
  // The filter suggestion that was accepted by the user and is being applied
  // by this navigation. Set only if `was_filter_initiated_navigation` is true.
  std::optional<UrlFilterSuggestion> applied_suggestion;
  // Whether the navigation is a back navigation (e.g., the user presses the
  // back button of the browser).
  bool is_back_navigation = false;
  // True if the navigation was initiated by the user typing in the omnibox
  // (address bar) or clicking a bookmark.
  bool is_navigation_from_omnibox_or_bookmarks = false;
  // The UKM source ID of the navigation.
  ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_FILTER_NAVIGATION_METADATA_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_FILTER_NAVIGATION_METADATA_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_FILTER_NAVIGATION_METADATA_H_

#include <optional>

#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "url/gurl.h"

namespace multistep_filter {

// Immutable snapshot of tab navigation state (network, security, and user
// gestures). Populated by the content layer and dispatched into the Core layer.
struct FilterNavigationMetadata {
  int64_t navigation_id = 0;
  GURL url;
  GURL prev_url;
  bool is_cryptographic_scheme = false;
  bool is_http_allowed_for_testing = false;
  int net_error_code = 0;
  int http_response_code = 0;
  bool is_error_page_navigation = false;
  bool has_user_gesture = false;
  bool was_filter_initiated_navigation = false;
  bool is_same_document_navigation = false;
  std::optional<UrlFilterSuggestion> applied_suggestion;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_FILTER_NAVIGATION_METADATA_H_

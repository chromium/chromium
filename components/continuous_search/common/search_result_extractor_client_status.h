// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTINUOUS_SEARCH_COMMON_SEARCH_RESULT_EXTRACTOR_CLIENT_STATUS_H_
#define COMPONENTS_CONTINUOUS_SEARCH_COMMON_SEARCH_RESULT_EXTRACTOR_CLIENT_STATUS_H_

namespace continuous_search {

// Keep values in this enum up to date with the entry for
// SearchResultExtractorClientStatus in enums.xml.

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.continuous_search
enum SearchResultExtractorClientStatus {
  kSuccess,
  kNoResults,
  kUnexpectedUrl,
  kWebContentsGone,
  kNativeNotInitialized,
  kAlreadyCapturing,
  kWebContentsHasNonSrpUrl,
  kNotEnoughResults,
  kMaxValue,
};

}  // namespace continuous_search

#endif  // COMPONENTS_CONTINUOUS_SEARCH_COMMON_SEARCH_RESULT_EXTRACTOR_CLIENT_STATUS_H_

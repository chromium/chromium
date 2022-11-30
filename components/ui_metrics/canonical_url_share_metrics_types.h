// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_METRICS_CANONICAL_URL_SHARE_METRICS_TYPES_H_
#define COMPONENTS_UI_METRICS_CANONICAL_URL_SHARE_METRICS_TYPES_H_

namespace ui_metrics {
// The histogram key to report the result of the Canonical URL retrieval.
const char kCanonicalURLResultHistogram[] = "Mobile.CanonicalURLResult";

// Used to report the result of the retrieval of the Canonical URL.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.ui_metrics
enum CanonicalURLResult {
  // The canonical URL retrieval failed because the visible URL is not HTTPS.
  FAILED_VISIBLE_URL_NOT_HTTPS = 0,

  // Deprecated.
  FAILED_CANONICAL_URL_NOT_HTTPS_DEPRECATED,

  // The canonical URL retrieval failed because the page did not define one.
  FAILED_NO_CANONICAL_URL_DEFINED,

  // The canonical URL retrieval failed because the page's canonical URL was
  // invalid.
  FAILED_CANONICAL_URL_INVALID,

  // The canonical URL retrieval succeeded. The retrieved canonical URL is
  // different from the visible URL.
  SUCCESS_CANONICAL_URL_DIFFERENT_FROM_VISIBLE,

  // The canonical URL retrieval succeeded. The retrieved canonical URL is the
  // same as the visible URL.
  SUCCESS_CANONICAL_URL_SAME_AS_VISIBLE,

  // The canonical URL retrieval succeeded. The canonical URL is not HTTPS
  // (but the visible URL is).
  SUCCESS_CANONICAL_URL_NOT_HTTPS,

  // The count of canonical URL results. This must be the last item in the enum.
  CANONICAL_URL_RESULT_COUNT
};
}  // namespace ui_metrics

#endif  // COMPONENTS_UI_METRICS_CANONICAL_URL_SHARE_METRICS_TYPES_H_

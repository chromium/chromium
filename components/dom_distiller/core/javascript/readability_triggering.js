// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Runs readability heuristic on the page and return the result.
(function(minScore, minContentLength) {
function initialize() {
  // This include will be processed at build time by grit.
  // clang-format off
        // <include src="../../../../third_party/readability/modded_src/Readability-readerable.js">
  // clang-format on
  window.isProbablyReaderable = isProbablyReaderable;
}
initialize();

return isProbablyReaderable(
    document, {minScore: minScore, minContentLength: minContentLength});
})($$MIN_SCORE_PLACEHOLDER, $$MIN_CONTENT_LENGTH_PLACEHOLDER);

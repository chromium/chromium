// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Runs readability heuristic on the page and return the result.
(function() {
function initialize() {
  // This include will be processed at build time by grit.
  // clang-format off
      // <include src="../../../../third_party/readability/src/Readability-readerable.js">
  // clang-format on
  window.isProbablyReaderable = isProbablyReaderable;
}
window.setTimeout = function() {};
window.clearTimeout = function() {};
initialize();

return isProbablyReaderable(document);
})();

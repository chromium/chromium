// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Runs readability heuristic on the page and return the result.
(function(allowedVideoRegex) {
try {
  function initialize() {
    // This include will be processed at build time by grit.
    // clang-format off
      // <include src="../../../../third_party/readability/modded_src/Readability.js">
    // clang-format on
    window.Readability = Readability;
  }
  initialize();

  const options = {};
  if (allowedVideoRegex) {
    options.allowedVideoRegex = new RegExp('//' + allowedVideoRegex, 'i');
  }
  const clonedDoc = document.cloneNode(/*deep=*/ true);
  const article = new Readability(clonedDoc, options).parse();
  return article;
} catch (e) {
  window.console.error('Error during distillation: ' + e);
  if (e.stack !== undefined) {
    window.console.error(e.stack);
  }
}
return undefined;
})($$ALLOWED_VIDEO_REGEX);

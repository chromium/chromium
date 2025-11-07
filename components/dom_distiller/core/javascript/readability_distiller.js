// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Runs readability heuristic on the page and return the result.
(function() {
try {
  function initialize() {
    // This include will be processed at build time by grit.
    // clang-format off
      // <include src="../../../../third_party/readability/modded_src/Readability.js">
    // clang-format on
    window.Readability = Readability;
  }
  initialize();

  const article = new Readability(document.cloneNode(/*deep=*/ true)).parse();
  return article;
} catch (e) {
  window.console.error('Error during distillation: ' + e);
  if (e.stack !== undefined) {
    window.console.error(e.stack);
  }
}
return undefined;
})();

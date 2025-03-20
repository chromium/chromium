// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Applies DomDistillerJs to the content of the page and returns a
// DomDistillerResults (as a javascript object/dict).
(function(options) {
try {
  function initialize() {
    // This include will be processed at build time by grit.
    // clang-format off
    // <include src="../../../../third_party/dom_distiller_js/dist/js/domdistiller.js">
    // clang-format on
  }
  window.setTimeout = function() {};
  window.clearTimeout = function() {};
  initialize();

  // The OPTIONS placeholder will be replaced with the DomDistillerOptions at
  // runtime.
  const distiller = window.org.chromium.distiller.DomDistiller;
  return distiller.applyWithOptions(options);

} catch (e) {
  window.console.error('Error during distillation: ' + e);
  if (e.stack !== undefined) {
    window.console.error(e.stack);
  }
}
return undefined;
})($$OPTIONS);

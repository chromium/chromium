// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Applies DomDistillerJs to the content of the page and returns a
// DomDistillerResults (as a javascript object/dict).
(function(options, stringify_output) {
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
  const res = distiller.applyWithOptions(options);

  if (stringify_output) {
    return JSON.stringify(res);
  }
  return res;
} catch (e) {
  window.console.error('Error during distillation: ' + e);
  if (e.stack !== undefined) {
    window.console.error(e.stack);
  }
}
return undefined;
})($$OPTIONS, $$STRINGIFY);

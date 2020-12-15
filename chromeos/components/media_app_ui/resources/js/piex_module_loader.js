// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {?Promise} */
let _piexLoadPromise = null;

/**
 * Loads PIEX, the "Preview Image Extractor", via wasm.
 * @return {!Promise}
 */
function loadPiex() {
  async function startLoad() {
    /** @type {function(string): !Promise} */
    const loadJs = (/** string */ path) => new Promise((resolve, reject) => {
      const script =
          /** @type {!HTMLScriptElement} */ (document.createElement('script'));
      script.onload = resolve;
      script.onerror = reject;
      script.src = path;
      assertCast(document.head).appendChild(script);
    });
    await loadJs('piex/piex.js.wasm');
    await loadJs('piex_module_scripts.js');
  }
  if (!_piexLoadPromise) {
    _piexLoadPromise = startLoad();
  }
  return _piexLoadPromise;
}

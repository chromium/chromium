// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /** @param {string} pathPrefix Prefix for the path to lazy_load.html */
  function ensureLazyLoaded(pathPrefix) {
    // Only trigger lazy loading, if we are in top-level Settings page.
    // IMPORTANT: This is used when running tests that use the Polymer Bundler
    // (aka vulcanize).
    if (location.href == location.origin + '/') {
      suiteSetup(function() {
        return new Promise(function(resolve, reject) {
          // This URL needs to match the URL passed to <settings-idle-load> from
          // <settings-basic-page>.
          const path = (pathPrefix || '') + '/lazy_load.html';
          Polymer.Base.importHref(path, resolve, reject, true);
        });
      });
    }
  }

  return {
    ensureLazyLoaded: ensureLazyLoaded,
  };
});

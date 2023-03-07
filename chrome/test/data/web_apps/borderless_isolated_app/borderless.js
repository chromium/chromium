// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// This string must match with
// `BorderlessIsolatedWebAppBrowserTest::kBorderlessAppOnloadTitle`.
window.onload = function() {
  document.title = 'Borderless';
};

const mql = window.matchMedia('(display-mode: borderless)');
mql.addEventListener('change', event => {
  if (event.matches) {
    document.title = 'match-media-borderless';
  } else {
    document.title = 'Borderless';  // The same title as set onload.
  }
});

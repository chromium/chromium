// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// This string must match with
// `UnframedIsolatedWebAppBrowserTest::kUnframedAppOnloadTitle`.
window.onload = function() {
  document.title = 'Unframed';
};

const mql = window.matchMedia('(display-mode: unframed)');
mql.addEventListener('change', event => {
  if (event.matches) {
    document.title = 'match-media-unframed';
  } else {
    document.title = 'Unframed';  // The same title as set onload.
  }
});

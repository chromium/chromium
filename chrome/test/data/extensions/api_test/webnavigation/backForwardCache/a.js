// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  if (window.sessionStorage['redirected'] != 1) {
    window.sessionStorage['redirected'] = 1;
    // Required so this results in a history entry being created.
    setTimeout(function() {
      location.href =
          "http://b.com:" + location.port +
          "/extensions/api_test/webnavigation/backForwardCache/b.html";
    }, 0);
  }
};

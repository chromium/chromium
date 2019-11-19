// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = function() {
  if (location.hash) {
    var completionUrl = new URL(location.hash.slice(1)).href;

    console.log(
        'Fake devtools loaded. Going to notify test extension via ' +
        completionUrl);

    // Cannot do "location.href = completionUrl" because devtools://...
    // disallows top-level navigation to a non-devtools:-URL.
    new Image().src = completionUrl;
  }
};

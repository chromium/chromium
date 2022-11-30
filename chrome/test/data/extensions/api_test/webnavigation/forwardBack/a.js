// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  if (window.sessionStorage['redirected'] != 1) {
    window.sessionStorage['redirected'] = 1;
    // Required so this results in a history entry being created.
    window.setTimeout(function() {document.location = 'b.html'}, 0);
  }
};

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var resolveEntereedFullscreen;
window.getEnteredFullscreen = new Promise((resolve) => {
  resolveEnteredFullscreen = resolve;
});

document.body.onclick = function() {
  document.body.requestFullscreen().then(() => {
    resolveEnteredFullscreen('success');
  }).catch(() => {
    resolveEnteredFullscreen('failure');
  });
};

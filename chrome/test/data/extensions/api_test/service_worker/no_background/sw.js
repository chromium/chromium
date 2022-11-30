// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.onmessage = function(e) {
  // Messages provide us with a MessagePort to post the result of
  // getBackgroundClient to.
  chrome.runtime.getBackgroundClient().then(function(client) {
    e.data.port.postMessage({success: true});
  }).catch(function(err) {
    e.data.port.postMessage({success: false, error: err});
  });
};

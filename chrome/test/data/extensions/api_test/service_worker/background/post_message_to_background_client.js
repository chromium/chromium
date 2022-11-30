// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

chrome.runtime.getBackgroundClient().then(function(client) {
  client.postMessage('success');
}).catch(function(error) {
  // This test passes, so logic never reaches here... but it would still be
  // nice to signal failure to the test. Unfortunately, without any extension
  // page to bounce off, we can't.
});

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.info('running extension!');
chrome.webNavigation.onBeforeNavigate.addListener(function(details) {
  console.info('Got the event!');
  chrome.test.succeed();
});

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Make sure that putting interceptors on event filter attributes doesn't
// break anything.
Object.defineProperty(Object.prototype, 'windowExposedByDefault',
                      {enumerable: true, get() { return 'hahaha'; }});
chrome.webNavigation.onBeforeNavigate.addListener(function() {
  chrome.test.notifyPass();
}, {url: [{hostContains: 'example.com'}]});

chrome.test.sendMessage('ready');

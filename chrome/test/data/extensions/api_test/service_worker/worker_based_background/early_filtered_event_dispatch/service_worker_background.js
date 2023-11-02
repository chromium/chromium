// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var isInstanceOfServiceWorkerGlobalScope =
    ('ServiceWorkerGlobalScope' in self) &&
    (self instanceof ServiceWorkerGlobalScope);

if (!isInstanceOfServiceWorkerGlobalScope) {
  chrome.test.sendMessage('FAIL');
} else {
  // The event is dispatched directly from the test EarlyFilteredEventDispatch.
  chrome.webNavigation.onCommitted.addListener(function(details) {
    chrome.test.sendMessage('PASS');
  }, {url: [{pathSuffix: 'a.html'}]});
}

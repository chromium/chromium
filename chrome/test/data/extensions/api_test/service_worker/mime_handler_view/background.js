// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var seenPathsByServiceWorker = [];

// Called by mime_handler.js at the end of the test:
chrome.runtime.onMessage.addListener(function(msg) {
  chrome.test.assertEq('finish test by checking SW URLs', msg);
  chrome.test.assertFalse(
    seenPathsByServiceWorker.includes("/well-known-mime.ics"));
  chrome.test.notifyPass();
});

navigator.serviceWorker.addEventListener('message', function(event) {
  seenPathsByServiceWorker.push(event.data.replace(location.origin, ''));
  event.ports[0].postMessage('ACK');
});

navigator.serviceWorker.register('sw.js').then(function() {
  return navigator.serviceWorker.ready;
}).then(function() {
  chrome.tabs.create({
    url: chrome.runtime.getURL('page_with_embed.html'),
  });
}).catch(function(e) {
  chrome.test.fail('Unexpected error: ' + e);
});

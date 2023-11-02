// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.addEventListener('fetch', function(event) {
  event.waitUntil(
    chrome.runtime.getBackgroundClient().then(function(client) {
      // Ensure that the "seenUrls" list in the background page is updated
      // before the response is served.
      return new Promise(function(resolve) {
        var chan = new MessageChannel();
        chan.port1.onmessage = resolve;
        client.postMessage(event.request.url, [chan.port2]);
      });
    }));
});

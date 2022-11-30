// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var isInstanceOfServiceWorkerGlobalScope =
    ('ServiceWorkerGlobalScope' in self) &&
    (self instanceof ServiceWorkerGlobalScope);

if (!isInstanceOfServiceWorkerGlobalScope) {
  chrome.test.sendMessage('FAIL');
} else {
  chrome.test.onMessage.addListener(args =>
      chrome.test.sendMessage(args.data == 'hello' ? 'PASS': 'FAIL'));
}

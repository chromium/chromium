// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const isInstanceOfServiceWorkerGlobalScope =
    ('ServiceWorkerGlobalScope' in self) &&
    (self instanceof ServiceWorkerGlobalScope);

chrome.tabs.onCreated.addListener(tab => {
  console.log('onCreated');
  console.log(tab.pendingUrl);
  var url = new URL(tab.pendingUrl);
  var isAboutBlank = url.href == 'about:blank';

  // Note: Ignore 'about:blank' navigations.
  if (url.pathname == '/extensions/test_file.html') {
    chrome.test.sendMessage('CREATED');
  } else if (url.href != 'about:blank') {
    chrome.test.sendMessage('CREATE_FAILED');
  }
});

chrome.test.sendMessage(
    isInstanceOfServiceWorkerGlobalScope ? 'WORKER_RUNNING'
                                         : 'NON_WORKER_SCOPE');

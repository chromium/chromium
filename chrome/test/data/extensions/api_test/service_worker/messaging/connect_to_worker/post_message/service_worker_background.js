// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onConnect.addListener(port => {
  port.onMessage.addListener(msg => {
    chrome.test.assertEq('tab->worker', msg);
    port.postMessage('tab->worker->tab');
  });
});

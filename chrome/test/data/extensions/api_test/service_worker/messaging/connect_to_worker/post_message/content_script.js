// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var port = chrome.runtime.connect();
port.onMessage.addListener(msg => {
  chrome.test.assertEq('tab->worker->tab', msg);
  chrome.test.succeed();
});
// Send message to extension SW which will reply back with the message
// 'tab->worker->tab'.
port.postMessage('tab->worker');

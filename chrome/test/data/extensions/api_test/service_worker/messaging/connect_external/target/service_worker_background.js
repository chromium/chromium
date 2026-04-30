// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onConnectExternal.addListener(port => {
  console.info('{worker} target got port connection');
  port.onMessage.addListener(msg => {
    console.info('target {worker}: got message: ' + msg);
    chrome.test.assertEq('initiator->target', msg);
    port.postMessage('initiator->target->initiator');
  });
});

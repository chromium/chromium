// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The extension resides in ../target/*
const kTargetExtensionId = 'pkplfbidichfdicaijlchgnapepdginl';

chrome.test.runTests([function testConnectExternal() {
  var port = chrome.runtime.connect(kTargetExtensionId);
  port.onMessage.addListener(msg => {
    console.log('{worker} initiator extension got message reply: ' + msg);
    chrome.test.assertEq('initiator->target->initiator', msg);
    chrome.test.succeed();
  });
  console.log('created port');
  port.postMessage('initiator->target');
}]);

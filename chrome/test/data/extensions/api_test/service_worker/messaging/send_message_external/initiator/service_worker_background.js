// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The extension resides in ../target/*
const kTargetExtensionId = 'pkplfbidichfdicaijlchgnapepdginl';

chrome.test.runTests([function testSendMessageExternal() {
  chrome.runtime.sendMessage(
      kTargetExtensionId, 'initiator->target', function(response) {
        console.log('Extension got response from other extension: ' + response);
        chrome.test.assertEq('initiator->target->initiator', response);
        chrome.test.succeed();
      });
}]);

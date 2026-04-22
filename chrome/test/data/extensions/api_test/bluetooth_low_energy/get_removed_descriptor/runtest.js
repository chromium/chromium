// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const getDescriptor = chrome.bluetoothLowEnergy.getDescriptor;
const descId = 'desc_id0';

getDescriptor(descId, function(result) {
  if (chrome.runtime.lastError) {
    chrome.test.sendMessage(chrome.runtime.lastError.message);
  }

  chrome.test.assertEq(descId, result.instanceId);

  chrome.test.sendMessage('ready', function(message) {
    getDescriptor(descId, function(result) {
      if (result || !chrome.runtime.lastError) {
        chrome.test.sendMessage('Call to getDescriptor should have failed');
      }

      chrome.test.sendMessage('ready', function(message) {
        chrome.test.succeed();
      });
    });
  });
});

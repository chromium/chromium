// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
function listener(data) {
  chrome.test.sendMessage('Event should not have been received.');
}

chrome.bluetoothLowEnergy.onServiceChanged.addListener(listener);
chrome.bluetoothLowEnergy.onCharacteristicValueChanged.addListener(listener);
chrome.bluetoothLowEnergy.onDescriptorValueChanged.addListener(listener);

chrome.test.sendMessage('ready', function (message) {
  chrome.test.succeed();
});

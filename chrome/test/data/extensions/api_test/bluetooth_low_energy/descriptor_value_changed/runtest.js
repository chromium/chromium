// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testDescriptorValueChanged() {
  chrome.test.assertEq(2, Object.keys(changedDescs).length);

  chrome.test.assertEq(descId0, changedDescs[descId0].instanceId);
  chrome.test.assertEq(descId1, changedDescs[descId1].instanceId);

  chrome.test.succeed();
}

var descId0 = 'desc_id0';
var descId1 = 'desc_id1';

var changedDescs = {}

chrome.bluetoothLowEnergy.onDescriptorValueChanged.addListener(function (desc) {
  changedDescs[desc.instanceId] = desc;
});

chrome.test.sendMessage('ready', function (message) {
  chrome.test.runTests([testDescriptorValueChanged]);
});

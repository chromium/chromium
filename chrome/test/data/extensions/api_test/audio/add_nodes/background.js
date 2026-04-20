// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function waitForDeviceChangedEventTests() {
    chrome.test.listenOnce(chrome.audio.onDeviceListChanged, function(devices) {
      var deviceList = devices.map(function(device) {
        return {
          id: device.id,
          stableDeviceId: device.stableDeviceId,
          streamType: device.streamType,
          deviceType: device.deviceType,
          deviceName: device.deviceName,
          displayName: device.displayName
        };
      }).sort(function(lhs, rhs) {
        return Number.parseInt(lhs.id) - Number.parseInt(rhs.id);
      });

     chrome.test.assertEq([{
        id: '30001',
        stableDeviceId: '0',
        streamType: 'OUTPUT',
        deviceType: 'USB',
        deviceName: 'Jabra Speaker',
        displayName: 'Jabra Speaker 1'
      }, {
        id: '30002',
        stableDeviceId: '1',
        streamType: 'OUTPUT',
        deviceType: 'USB',
        deviceName: 'Jabra Speaker',
        displayName: 'Jabra Speaker 2'
      }, {
        id: '30003',
        stableDeviceId: '2',
        streamType: 'OUTPUT',
        deviceType: 'HDMI',
        deviceName: 'HDMI output',
        displayName: 'HDA Intel MID'
      }], deviceList);
    });
  }
]);

chrome.test.sendMessage('loaded');

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testSetDiscoveryFilter() {
  // Pre-set discovery filter
  chrome.bluetoothPrivate.setDiscoveryFilter(
    {
      uuids: ["cafe", "0000bebe-0000-1000-8000-00805f9b34fb"],
      transport: "le",
      pathloss: 50
    },
    function() {
      chrome.test.assertNoLastError();
      // Start discovery with pre-set filter.
      chrome.bluetooth.startDiscovery(function(){
        chrome.test.assertNoLastError();

        // Change filter (clear) during scan.
        chrome.bluetoothPrivate.setDiscoveryFilter({}, function() {
          chrome.test.assertNoLastError();
          // Success.
          chrome.test.succeed();
        });
      });
  });
}

chrome.test.runTests([ testSetDiscoveryFilter ]);

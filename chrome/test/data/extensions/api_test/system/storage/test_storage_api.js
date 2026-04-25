// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// system.storage api test
// extensions_browsertests --gtest_filter=SystemStorageApiTest.Storage

// Testing data should be the same as |kTestingData| in
// system_storage_apitest.cc.
var testData = [
  { id:"", name: "0xbeaf", type: "removable", capacity: 4098,
    availableCapacity: 1},
  { id:"", name: "/home", type: "fixed", capacity: 4098,
    availableCapacity: 2},
  { id:"", name: "/data", type: "fixed", capacity: 10000,
    availableCapacity: 3}
];

chrome.test.runTests([
  function testGetInfo() {
    chrome.system.storage.getInfo(chrome.test.callbackPass(function(units) {
      chrome.test.assertTrue(units.length == 3);
      for (var i = 0; i < units.length; ++i) {
        chrome.test.sendMessage(units[i].id);
        chrome.test.assertEq(testData[i].name, units[i].name);
        chrome.test.assertEq(testData[i].type, units[i].type);
        chrome.test.assertEq(testData[i].capacity, units[i].capacity);
      }
    }));
  },
  function testGetAvailableCapacity() {
    chrome.system.storage.getInfo(chrome.test.callbackPass(function(units) {
      chrome.test.assertTrue(units.length == 3);
      // Record all storage devices' |id| in testData.
      for (var i = 0; i < units.length; ++i)
        testData[i].id = units[i].id;
      for (var i = 0; i < units.length; ++i) {
        chrome.system.storage.getAvailableCapacity(units[i].id, function(info) {
          for (var j = 0; j < units.length; ++j) {
            if (info.id == testData[j].id) {
              chrome.test.assertEq(testData[j].availableCapacity,
                                   info.availableCapacity);
            }
          }
        });
      }
    }));
  }
]);

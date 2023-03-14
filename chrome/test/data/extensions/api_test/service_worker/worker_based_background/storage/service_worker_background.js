// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testSetStorage = function(storageArea, key, value) {
  var options = {};
  options[key] = value;
  try {
    storageArea.set(options, function() {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  }
  catch (e) {
    chrome.test.fail(e);
  }
};

var testGetStorage = function(storageArea, key, expectedValue) {
  try {
    storageArea.get([key], function(result) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq(expectedValue, result[key]);
      chrome.test.succeed();
    });
  }
  catch (e) {
    chrome.test.fail(e);
  }
};

var testGetStorageBytesInUse = function(storageArea, key) {
  try {
    storageArea.getBytesInUse([key], function(bytes) {
      chrome.test.assertNoLastError();
      chrome.test.assertNe(0, bytes);
      chrome.test.succeed();
    });
  }
  catch (e) {
    chrome.test.fail(e);
  }
};

var testRemoveStorage = function(storageArea, key) {
  try {
    storageArea.remove([key], function(result) {
      chrome.test.assertNoLastError();
      storageArea.get([key], function(result) {
        chrome.test.assertNoLastError();
        chrome.test.assertEq({}, result);
        chrome.test.succeed();
      });
    });
  }
  catch (e) {
    chrome.test.fail(e);
  }
};

var testClearStorage = function(storageArea, key) {
  try {
    storageArea.clear(function() {
      chrome.test.assertNoLastError();
      storageArea.get([key], function(result) {
        chrome.test.assertNoLastError();
        chrome.test.assertEq({}, result);
        chrome.test.succeed();
      });
    });
  }
  catch (e) {
    chrome.test.fail(e);
  }
};

var testOnStorageChanged = function(storageArea) {
  try {
    var changedKey = '_changed_key';
    var changedValue = 'changed_value';
    storageArea.onChanged.addListener(function callback(changes) {
      storageArea.onChanged.removeListener(callback);
      chrome.test.assertNoLastError();
      chrome.test.assertEq(changes[changedKey].newValue, changedValue);
      chrome.test.succeed();
    });
    var options = {};
    options[changedKey] = changedValue;
    storageArea.set(options);
  }
  catch (e) {
    chrome.test.fail(e);
  }
};

let namespaces = [
  {
    storage_area: chrome.storage.local,
    key: '_local_key',
    value: 'this is a local value',
  },
  {
    storage_area: chrome.storage.sync,
    key: '_sync_key',
    value: 'this is a sync value',
  },
  {
    'storage_area': chrome.storage.session,
    'key': '_session_key',
    'value': 'this is a session value',
  }
];

let tests = [];
for (const namespace of namespaces) {
  tests.push(
      function testSet() {
        testSetStorage(namespace.storage_area, namespace.key, namespace.value);
      },
      function testGet() {
        testGetStorage(namespace.storage_area, namespace.key, namespace.value);
      },
      function testGetBytesInUse() {
        testGetStorageBytesInUse(namespace.storage_area, namespace.key);
      },
      function testRemove() {
        testRemoveStorage(namespace.storage_area, namespace.key);
      },
      function testClearSetup() {
        testSetStorage(namespace.storage_area, namespace.key, namespace.value);
      },
      function testClear() {
        testClearStorage(namespace.storage_area, namespace.key);
      },
      function testChanges() {
        testOnStorageChanged(namespace.storage_area);
      })
}

chrome.test.runTests(tests);

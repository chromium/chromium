// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const testSetStorage = function(storageArea, key, value) {
  const options = {};
  options[key] = value;
  try {
    storageArea.set(options, function() {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  } catch (e) {
    chrome.test.fail(e);
  }
};

const testGetStorage = function(storageArea, key, expectedValue) {
  try {
    storageArea.get([key], function(result) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq(expectedValue, result[key]);
      chrome.test.succeed();
    });
  } catch (e) {
    chrome.test.fail(e);
  }
};

const testGetStorageBytesInUse = function(storageArea, key) {
  try {
    storageArea.getBytesInUse([key], function(bytes) {
      chrome.test.assertNoLastError();
      chrome.test.assertNe(0, bytes);
      chrome.test.succeed();
    });
  } catch (e) {
    chrome.test.fail(e);
  }
};

const testRemoveStorage = function(storageArea, key) {
  try {
    storageArea.remove([key], function(result) {
      chrome.test.assertNoLastError();
      storageArea.get([key], function(result) {
        chrome.test.assertNoLastError();
        chrome.test.assertEq({}, result);
        chrome.test.succeed();
      });
    });
  } catch (e) {
    chrome.test.fail(e);
  }
};

const testClearStorage = function(storageArea, key) {
  try {
    storageArea.clear(function() {
      chrome.test.assertNoLastError();
      storageArea.get([key], function(result) {
        chrome.test.assertNoLastError();
        chrome.test.assertEq({}, result);
        chrome.test.succeed();
      });
    });
  } catch (e) {
    chrome.test.fail(e);
  }
};

const testOnStorageChanged = function(storageArea) {
  try {
    const changedKey = '_changed_key';
    const changedValue = 'changed_value';
    storageArea.onChanged.addListener(function callback(changes) {
      storageArea.onChanged.removeListener(callback);
      chrome.test.assertNoLastError();
      chrome.test.assertEq(changes[changedKey].newValue, changedValue);
      chrome.test.succeed();
    });
    const options = {};
    options[changedKey] = changedValue;
    storageArea.set(options);
  } catch (e) {
    chrome.test.fail(e);
  }
};

const namespaces = [
  {
    storageArea: chrome.storage.local,
    key: '_local_key',
    value: 'this is a local value',
  },
  {
    storageArea: chrome.storage.sync,
    key: '_sync_key',
    value: 'this is a sync value',
  },
  {
    storageArea: chrome.storage.session,
    key: '_session_key',
    value: 'this is a session value',
  },
];

const tests = [];
for (const namespace of namespaces) {
  tests.push(
      function testSet() {
        testSetStorage(namespace.storageArea, namespace.key, namespace.value);
      },
      function testGet() {
        testGetStorage(namespace.storageArea, namespace.key, namespace.value);
      },
      function testGetBytesInUse() {
        testGetStorageBytesInUse(namespace.storageArea, namespace.key);
      },
      function testRemove() {
        testRemoveStorage(namespace.storageArea, namespace.key);
      },
      function testClearSetup() {
        testSetStorage(namespace.storageArea, namespace.key, namespace.value);
      },
      function testClear() {
        testClearStorage(namespace.storageArea, namespace.key);
      },
      function testChanges() {
        testOnStorageChanged(namespace.storageArea);
      });
}

chrome.test.runTests(tests);

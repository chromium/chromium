// Copyright 2019 The Chromium Authors. All rights reserved.
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
      chrome.test.assertFalse(bytes == 0);
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

var localKey = '_local_key';
var localValue = 'this is a local value';
var syncKey = '_sync_key';
var syncValue = 'this is a sync value';
var sessionKey = '_session_key';
var sessionValue = 'this is a session value';

chrome.test.runTests([
  function testLocalSet() {
    testSetStorage(chrome.storage.local, localKey, localValue);
  },
  function testLocalGet() {
    testGetStorage(chrome.storage.local, localKey, localValue);
  },
  function testLocalGetBytesInUse() {
    testGetStorageBytesInUse(chrome.storage.local, localKey);
  },
  function testLocalRemove() {
    testRemoveStorage(chrome.storage.local, localKey);
  },
  function testLocalClearSetup() {
    testSetStorage(chrome.storage.local, localKey, localValue);
  },
  function testLocalClear() {
    testClearStorage(chrome.storage.local, localKey);
  },
  function testLocalOnStorageChanged() {
    testOnStorageChanged(chrome.storage.local);
  },
  function testSyncSet() {
    testSetStorage(chrome.storage.sync, syncKey, syncValue);
  },
  function testSyncGet() {
    testGetStorage(chrome.storage.sync, syncKey, syncValue);
  },
  function testSyncGetBytesInUse() {
    testGetStorageBytesInUse(chrome.storage.sync, syncKey);
  },
  function testSyncRemove() {
    testRemoveStorage(chrome.storage.sync, syncKey);
  },
  function testSyncClearSetup() {
    testSetStorage(chrome.storage.sync, syncKey, syncValue);
  },
  function testSyncClear() {
    testClearStorage(chrome.storage.sync, syncKey);
  },
  function testSyncOnStorageChanged() {
    testOnStorageChanged(chrome.storage.sync);
  },
  function testSessionSet() {
    testSetStorage(chrome.storage.session, sessionKey, sessionValue);
  },
  function testSessionGet() {
    testGetStorage(chrome.storage.session, sessionKey, sessionValue);
  },
]);

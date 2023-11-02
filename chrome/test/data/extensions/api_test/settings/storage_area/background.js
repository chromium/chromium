// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var storageAreaOnChanged = function() {
  // Checks the onChanged callback is called from both StorageArea and
  // chrome.storage.
  var localStorageArea = chrome.storage.local;
  chrome.test.listenOnce(localStorageArea.onChanged, function(changes) {
    chrome.test.assertEq({key:{newValue:'value'}}, changes);
  });

  chrome.test.listenOnce(chrome.storage.onChanged,
    function(changes, namespace) {
      chrome.test.assertEq({key:{newValue:'value'}}, changes);
      chrome.test.assertEq('local', namespace);
    }
  );

  chrome.storage.managed.onChanged.addListener(function(changes, namespace) {
    chrome.test.notifyFail('managed.onChanged should not be called when local '
                           + 'storage update');
  });

  chrome.storage.sync.onChanged.addListener(function(changes, namespace) {
    chrome.test.notifyFail('sync.onChanged should not be called when local ' +
                           'storage update');
  });

  chrome.storage.session.onChanged.addListener(function(changes, namespace) {
    chrome.test.notifyFail(
        'session.onChanged should not be called when local storage update');
  });

  localStorageArea.set({key: 'value'});
};

chrome.test.runTests([storageAreaOnChanged]);

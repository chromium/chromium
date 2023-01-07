// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Retains a test directory.
 * @return {Promise} Promise fulflled/rejected depending on the test result.
 */
function retainDirectory() {
  return new Promise(function(fulfill) {
    chrome.app.window.create('window.html', fulfill);
  }).then(function(appWindow) {
    return new Promise(function(fulfill, rejected) {
      appWindow.contentWindow.chrome.fileSystem.chooseEntry(
          {type: "openDirectory"},
          fulfill);
    });
  }).then(function(selected) {
    chrome.test.assertTrue(selected.isDirectory);
    var id = chrome.fileSystem.retainEntry(selected);
    chrome.test.assertTrue(!!id);
    return new Promise(function(fulfill, rejected) {
      chrome.fileSystem.isRestorable(id, fulfill);
    }).then(function(restorable) {
      chrome.test.assertTrue(restorable);
      return new Promise(function(fulfill, rejected) {
        chrome.storage.local.set({id: id}, fulfill);
      });
    });
  }).then(function() {
    chrome.runtime.reload();
  });
}

/**
 * Restores a test directory.
 * @param {string} id ID of the test directory.
 * @return {Promise} Promise fulflled/rejected depending on the test result.
 */
function restoreDirectory(id) {
  return new Promise(function(fulfill) {
    chrome.fileSystem.isRestorable(id, fulfill);
  }).then(function(restorable) {
    chrome.test.assertTrue(restorable);
    return new Promise(function(fulfill) {
      chrome.fileSystem.restoreEntry(id, fulfill);
    });
  }).then(function(directory) {
    chrome.test.assertTrue(!!directory);
    chrome.test.assertTrue(!!directory.isDirectory);
  });
}

/**
 * Tests to retain and to restore directory on the drive.
 */
function testRetainEntry() {
  new Promise(function(fulfill) {
    chrome.storage.local.get('id', fulfill);
  }).then(function(values) {
    if (!values.id)
      return retainDirectory();
    else
      return restoreDirectory(values.id).then(chrome.test.callbackPass());
  }).catch(function(error) {
    chrome.test.fail(error.stack || error);
  });
}

chrome.test.runTests([testRetainEntry]);

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.sendMessage("Launched");

function requestSyncFS() {
  return new Promise(function(resolve, reject) {
    chrome.syncFileSystem.requestFileSystem(function(fs) {
      if (fs) {
        resolve(fs);
      } else {
        reject(chrome.runtime.lastError);
      }
    });
  });
}

function synchronizeToTestHarness(message) {
  return function() { return new Promise(function(resolve) {
      chrome.test.sendMessage('checkpoint: ' + message, resolve);
    });
  };
}

var fs = null;
requestSyncFS()
.then(function() {
  chrome.test.fail('Unexpected requestSyncFS success');
})
.catch(synchronizeToTestHarness('Failed to get syncfs'))
.then(requestSyncFS)
.then(function(fs_) {
  fs = fs_;
  return new Promise(function(resolve, reject) {
    fs.root.getFile('/foo', {create: true}, resolve, reject);
  });
})
.then(synchronizeToTestHarness('"/foo" created'))
.then(function() {
  return new Promise(function(resolve, reject) {
    fs.root.getFile('/bar', {create: true}, resolve, reject);
  });
})
.then(synchronizeToTestHarness('"/bar" created'))
.then(function() {
  chrome.test.succeed();
}).catch(function() {
  chrome.test.fail();
});

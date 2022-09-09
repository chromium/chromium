// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var FILE_NAME = 'data.txt';
var DATA = 'Hello world';

function getErrorHandler(message) {
  return function(e) {
    chrome.test.notifyFail(message + ', e.code=' + e.code);
    window.close();
  };
}

function writeData() {
  navigator.webkitPersistentStorage.requestQuota(1024, function(bytes) {
    window.webkitRequestFileSystem(window.PERSISTENT, bytes, function(fs) {
      fs.root.getFile(FILE_NAME, {create: true}, function(fileEntry) {
        fileEntry.createWriter(function(fileWriter) {
          fileWriter.onwriteend = function(e) {
            chrome.test.notifyPass();
            window.close();
          };

          fileWriter.onerror = function(e) {
            chrome.test.notifyFail('Write failed: ' + e.toString());
          };

          var blob = new Blob([DATA], {type: 'text/plain'});

          fileWriter.write(blob);
        }, getErrorHandler('Failed fileEntry.createWriter'));
      }, getErrorHandler('Failed fs.root.getFile'));
    }, getErrorHandler('Failed webkitRequestFileSystem'));
  }, getErrorHandler('Failed webkitPersistentStorage.requestQuota'));
}

window.addEventListener('load', writeData);

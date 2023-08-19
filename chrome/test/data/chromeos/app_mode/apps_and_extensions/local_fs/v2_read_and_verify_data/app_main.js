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

function readAndVerifyData() {
  navigator.webkitPersistentStorage.requestQuota(1024, function(bytes) {
    window.webkitRequestFileSystem(window.PERSISTENT, bytes, function(fs) {
      fs.root.getFile(FILE_NAME, {}, function(fileEntry) {
        fileEntry.file(function(file) {
           var reader = new FileReader();
           reader.onloadend = function(e) {
             if (this.result == DATA)
               chrome.test.notifyPass();
             else
               chrome.test.notifyFail('Unexpected data=' + this.result);
             window.close();
           };

           reader.readAsText(file);
        }, getErrorHandler('Failed fileEntry.file'));
      }, getErrorHandler('Failed fs.root.getFile'));
    }, getErrorHandler('Failed webkitRequestFileSystem'));
  }, getErrorHandler('Failed webkitPersistentStorage.requestQuota'));
}

window.addEventListener('load', readAndVerifyData);

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var EXTENSION_ID = 'pkplfbidichfdicaijlchgnapepdginl';
var FILE_CONTENTS = 'hello from test extension.';

function errorCallback(error) {
  var msg = '';
  if (!error.code) {
    msg = error.message;
  } else {
    switch (error.code) {
      case FileError.QUOTA_EXCEEDED_ERR:
        msg = 'QUOTA_EXCEEDED_ERR';
        break;
      case FileError.NOT_FOUND_ERR:
        msg = 'NOT_FOUND_ERR';
        break;
      case FileError.SECURITY_ERR:
        msg = 'SECURITY_ERR';
        break;
      case FileError.INVALID_MODIFICATION_ERR:
        msg = 'INVALID_MODIFICATION_ERR';
        break;
      case FileError.INVALID_STATE_ERR:
        msg = 'INVALID_STATE_ERR';
        break;
      default:
        msg = 'Unknown Error';
        break;
    };
  }

  chrome.test.fail(msg);
}

function ensureFileExists(entry, successCallback, errorCallback) {
  entry.filesystem.root.getFile(entry.fullPath,
                                {create: true},
                                successCallback,
                                errorCallback);
}

function writeToFile(entry) {
  entry.createWriter(function(writer) {
    writer.onerror = function(e) {
      errorCallback(writer.error);
    };
    writer.onwrite = chrome.test.succeed;

    var blob = new Blob([FILE_CONTENTS], {type: 'text/plain'});
    writer.write(blob);
  }, errorCallback);
}

chrome.test.runTests([
  function selectionSuccessful() {
    // The test will call selectFile function and expect it to succeed.
    // When it gets the file entry, it verifies that the permissions given in
    // the method allow the extension to read/write to selected file.
    chrome.fileBrowserHandler.selectFile(
        { suggestedName: 'some_file_name.txt',
          allowedFileExtensions: ['txt', 'html'] },
        function(result) {
          chrome.test.assertTrue(!!result);
          chrome.test.assertTrue(result.success);
          chrome.test.assertTrue(!!result.entry);

          ensureFileExists(result.entry, writeToFile, errorCallback);
      });
  },
  function selectionFails() {
    // The test expects that selectFile returns failure with an empty entry.
    chrome.fileBrowserHandler.selectFile({ suggestedName: 'fail' },
        function(result) {
          chrome.test.assertTrue(!!result);
          // Entry should be set iff operation succeeded.
          chrome.test.assertEq(false, result.success);
          chrome.test.assertTrue(result.entry == null);
          chrome.test.succeed();
        });
  }]);

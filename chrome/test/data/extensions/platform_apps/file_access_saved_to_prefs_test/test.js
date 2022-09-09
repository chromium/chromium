// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var textToWrite = 'def';

function truncateAndWriteToFile(writableEntry, callback) {
  writableEntry.createWriter(function(fileWriter) {
    fileWriter.onerror = function(e) {
      console.error("Couldn't write file: " + e.toString());
    };
    fileWriter.onwriteend = function(e) {
      fileWriter.onwriteend = function(e) {
        callback();
      };
      var blob = new Blob([textToWrite], {type: 'text/plain'});
      fileWriter.write(blob);
    };
    fileWriter.truncate(0);
  });
}

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create('index.html', {width: 100, height: 100},
      function(win) {
    var fs = win.contentWindow.chrome.fileSystem;
    fs.chooseEntry({type: 'openFile'}, function(entry) {
      chrome.fileSystem.retainEntry(entry);
      fs.getWritableEntry(entry, function(writableEntry) {
        chrome.fileSystem.retainEntry(writableEntry);
        truncateAndWriteToFile(writableEntry, function() {
          chrome.test.sendMessage('fileWritten');
          win.close();
        });
      });
    });
  });
});

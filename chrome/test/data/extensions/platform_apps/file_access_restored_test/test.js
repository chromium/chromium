// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var expectedText = 'def';

function truncateAndWriteToFile(writableEntry, callback) {
  writableEntry.createWriter(function(fileWriter) {
    fileWriter.onerror = function(e) {
      console.error("Couldn't write file: " + e.toString());
    };
    fileWriter.onwriteend = function(e) {
      fileWriter.onwriteend = function(e) {
        callback();
      };
      var blob = new Blob([expectedText], {type: 'text/plain'});
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
      fs.getWritableEntry(entry, function(writableEntry) {
        var id = fs.retainEntry(entry);
        chrome.storage.local.set({id:id}, function() {
          truncateAndWriteToFile(writableEntry, function() {
            chrome.test.sendMessage('fileWritten');
            win.close();
          });
        });
      });
    });
  });
});

chrome.app.runtime.onRestarted.addListener(function() {
  chrome.storage.local.get(null, function(data) {
    chrome.fileSystem.restoreEntry(data.id, function(entry) {
      if (!entry) {
        console.error("couldn't get file entry " + data.id);
        return;
      }
      entry.file(function(file) {
        var fr = new FileReader();
        fr.onload = function(e) {
          if (e.target.result != expectedText) {
            console.error("expected '" + expectedText + "', got '" +
              e.target.result + "'");
            return;
          }
          entry.createWriter(function(fileWriter) {
            fileWriter.onwriteend = function(e) {
              chrome.test.sendMessage('restartedFileAccessOK');
              win.close();
            };
            fileWriter.onerror = function(e) {
              console.error('Write failed: ' + e.toString());
            };
            var blob = new Blob(["doesn't matter"], {type: 'text/plain'});
            fileWriter.write(blob);
          });
        };
        fr.onerror = function(e) {
          chrome.test.fail("error reading file");
        };
        fr.readAsText(file);
      });
    });
  });
});

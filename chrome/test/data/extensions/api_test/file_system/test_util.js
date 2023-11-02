// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a duplicate of the file test_util in
// chrome/test/data/extensions/api_test/file_system

function checkEntry(entry, expectedName, isNew, shouldBeWritable) {
  chrome.test.assertEq(expectedName, entry.name);

  // Test that we are writable (or not), as expected.
  chrome.fileSystem.isWritableEntry(entry, chrome.test.callbackPass(
      function(isWritable) {
    chrome.test.assertEq(isWritable, shouldBeWritable);
  }));

  // Test that the file can be read.
  entry.file(chrome.test.callback(function(file) {
    var reader = new FileReader();
    reader.onloadend = chrome.test.callbackPass(function(e) {
      if (isNew)
        chrome.test.assertEq(reader.result, "");
      else
        chrome.test.assertEq(reader.result.indexOf("Can you see me?"), 0);
      // Test that we can write to the file, or not, depending on
      // |shouldBeWritable|.
      entry.createWriter(function(fileWriter) {
        fileWriter.onwriteend = chrome.test.callback(function(e) {
          if (fileWriter.error) {
            if (shouldBeWritable) {
              chrome.test.fail("Error writing to file: " +
                               fileWriter.error.toString());
            } else {
              chrome.test.succeed();
            }
          } else {
            if (shouldBeWritable) {
              // Get a new entry and check the data got to disk.
              chrome.fileSystem.chooseEntry(chrome.test.callbackPass(
                  function(readEntry) {
                readEntry.file(chrome.test.callback(function(readFile) {
                  var readReader = new FileReader();
                  readReader.onloadend = function(e) {
                    chrome.test.assertEq(readReader.result.indexOf("HoHoHo!"),
                                         0);
                    chrome.test.succeed();
                  };
                  readReader.onerror = function(e) {
                    chrome.test.fail("Failed to read file after write.");
                  };
                  readReader.readAsText(readFile);
                }));
              }));
            } else {
              chrome.test.fail(
                 "'Could write to file that should not be writable.");
            }
          }
        });
        var blob = new Blob(["HoHoHo!"], {type: "text/plain"});
        fileWriter.write(blob);
      });
    });
    reader.onerror = chrome.test.callback(function(e) {
      chrome.test.fail("Error reading file contents.");
    });
    reader.readAsText(file);
  }));
}

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Valid WEBP image.
var validWEBPImageCase = {
  filename: "valid.webp",
  blobString: "RIFF0\0\0\0WEBPVP8 $\0\0\0\xB2\x02\0\x9D\x01\x2A" +
              "\x01\0\x01\0\x2F\x9D\xCE\xE7s\xA8((((\x01\x9CK(\0" +
              "\x05\xCE\xB3l\0\0\xFE\xD8\x80\0\0"
}

// Write an invalid test image and expect failure.
var invalidWEBPImageCase = {
  filename: "invalid.webp",
  blobString: "abc123"
}

function runCopyToTest(testCase, expectSucceed) {
  var galleries;
  var testImageFileEntry;

  chrome.mediaGalleries.getMediaFileSystems(testGalleries);

  function testGalleries(results) {
    galleries = results;
    chrome.test.assertTrue(galleries.length > 0,
                           "Need at least one media gallery to test copyTo");

    // Create a temporary file system and an image for copying-into test.
    window.webkitRequestFileSystem(window.TEMPORARY, 1024*1024,
                                   temporaryFileSystemCallback,
                                   chrome.test.fail);
  }

  function temporaryFileSystemCallback(filesystem) {
    filesystem.root.getFile(testCase.filename, {create:true, exclusive: false},
                            temporaryImageCallback,
                            chrome.test.fail);
  }

  function temporaryImageCallback(entry) {
    testImageFileEntry = entry;
    entry.createWriter(imageWriterCallback, chrome.test.fail);
  }

  function imageWriterCallback(writer) {
    // Go through a Uint8Array to avoid UTF-8 control bytes.
    var blobBytes = new Uint8Array(testCase.blobString.length);
    for (var i = 0; i < testCase.blobString.length; i++) {
      blobBytes[i] = testCase.blobString.charCodeAt(i);
    }
    var blob = new Blob([blobBytes], {type : "image/webp"});

    writer.onerror = function(e) {
      chrome.test.fail("Unable to write test image: " + e.toString());
    }

    writer.onwriteend = testCopyTo(testImageFileEntry, galleries[0],
                                   testCase.filename, expectSucceed);

    writer.write(blob);
  }

  function testCopyTo(testImageFileEntry, gallery, filename, expectSucceed) {
    var onSuccess;
    var onFailure;
    if (expectSucceed) {
      onSuccess = chrome.test.succeed;
      onFailure = chrome.test.fail;
    } else {
      onSuccess = chrome.test.fail;
      onFailure = chrome.test.succeed;
    }
    return function() {
      testImageFileEntry.copyTo(gallery.root, filename, onSuccess, onFailure);
    };
  }
}

// The custom callback functions are options.
// If they are undefined, the default callbacks and checks will be used.
// If passed in, all 3 function arguments need to be valid.
function runReadGalleriesTest(expectedGalleryCount, expectSucceed,
                              customReadDirectoryCallback,
                              customReadDirectoryErrorCallback,
                              customGotGalleriesCallback) {
  var galleries;
  var readEntriesResults = [];
  var readDirectoryCallback;
  var readDirectoryErrorCallback;
  var gotGalleriesCallback;

  if (customReadDirectoryCallback && customReadDirectoryErrorCallback &&
      customGotGalleriesCallback) {
    chrome.test.assertEq(typeof(customReadDirectoryCallback), 'function');
    chrome.test.assertEq(typeof(customReadDirectoryErrorCallback), 'function');
    chrome.test.assertEq(typeof(customGotGalleriesCallback), 'function');
    readDirectoryCallback = customReadDirectoryCallback;
    readDirectoryErrorCallback = customReadDirectoryErrorCallback;
    gotGalleriesCallback = customGotGalleriesCallback;
  } else {
    chrome.test.assertTrue(!customReadDirectoryCallback &&
                           !customReadDirectoryErrorCallback &&
                           !customGotGalleriesCallback);
    readDirectoryCallback = defaultReadDirectoryCallback;
    readDirectoryErrorCallback = defaultReadDirectoryErrorCallback;
    gotGalleriesCallback = defaultGotGalleriesCallback;
  }
  chrome.mediaGalleries.getMediaFileSystems(testGalleries);

  function testGalleries(results) {
    gotGalleriesCallback(results);
    chrome.test.assertEq(expectedGalleryCount, results.length,
                         "Gallery count mismatch");
    if (expectedGalleryCount == 0) {
      chrome.test.succeed();
      return;
    }

    for (var i = 0; i < results.length; i++) {
      var dirReader = results[i].root.createReader();
      dirReader.readEntries(readDirectoryCallback, readDirectoryErrorCallback);
    }
  }

  function defaultGotGalleriesCallback(entries) {
    galleries = entries;
  }

  function defaultReadDirectoryCallback(entries) {
    var result = "";
    if (!expectSucceed) {
      result = "Unexpected readEntries success";
    }
    readEntriesResults.push(result);
    checkReadEntriesFinished();
  }

  function defaultReadDirectoryErrorCallback(err) {
    var result = "";
    if (expectSucceed) {
      result = "Unexpected readEntries failure: " + err;
    }
    readEntriesResults.push(result);
    checkReadEntriesFinished();
  }

  function checkReadEntriesFinished() {
    if (readEntriesResults.length != galleries.length)
      return;
    var success = true;
    for (var i = 0; i < readEntriesResults.length; i++) {
      if (readEntriesResults[i]) {
        success = false;
      }
    }
    if (success) {
      chrome.test.succeed();
      return;
    }
    chrome.test.fail(readEntriesResults);
  }
}

function checkMetadata(metadata) {
  chrome.test.assertNe(null, metadata);
  chrome.test.assertTrue(metadata.name.length > 0);
  chrome.test.assertTrue(metadata.galleryId.length > 0);
  chrome.test.assertTrue("isAvailable" in metadata);
  chrome.test.assertTrue("isMediaDevice" in metadata);
  chrome.test.assertTrue("isRemovable" in metadata);
  if (metadata.isRemovable && metadata.isAvailable) {
    chrome.test.assertTrue("deviceId" in metadata);
    chrome.test.assertTrue(metadata.deviceId.length > 0);
  }
}

// Gets the entire listing from directory, then verifies the sorted contents.
function verifyDirectoryEntry(directoryEntry, verifyFunction) {
  var allEntries = [];
  var reader = directoryEntry.createReader();

  function readEntries() {
    reader.readEntries(readEntriesCallback, chrome.test.fail);
  }

  function readEntriesCallback(entries) {
    if (entries.length == 0) {
      // This is the readEntries() is finished case.
      verifyFunction(directoryEntry, allEntries.sort());
      return;
    }

    allEntries = allEntries.concat(entries);
    readEntries();
  }

  readEntries();
}

function verifyJPEG(parentDirectoryEntry, filename, expectedFileLength,
                    doneCallback) {
  function verifyFileEntry(fileEntry) {
    fileEntry.file(verifyFile, chrome.test.fail);
  }

  function verifyFile(file) {
    var reader = new FileReader();

    reader.onload = function(e) {
      var arraybuffer = e.target.result;
      chrome.test.assertEq(expectedFileLength, arraybuffer.byteLength);
      doneCallback();
    };

    reader.onerror = function(e) {
      chrome.test.fail("Unable to read test image: " + filename);
    };

    reader.readAsArrayBuffer(file);
  }

  parentDirectoryEntry.getFile(filename, {create: false}, verifyFileEntry,
                               chrome.test.fail);
}

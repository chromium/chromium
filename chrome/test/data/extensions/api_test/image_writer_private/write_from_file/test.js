// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function assertNotNullOrUndefined(value, message) {
  chrome.test.assertNe(null, value, message);
  chrome.test.assertNe(undefined, value, message);
}

function testWriteFromFile() {
  var fileEntry;
  var storageDevice;
  var currentStage = "none";
  var currentProgress = -1;
  var started = true;

  function chooseEntryCallback(entry) {
    fileEntry = entry;

    chrome.imageWriterPrivate.listRemovableStorageDevices(
        listDevicesCallback);
  }

  function listDevicesCallback(deviceList) {
    chrome.test.assertTrue(deviceList.length >= 1);
    storageDevice = deviceList[0];

    startWrite();
  }

  function startWrite() {
    assertNotNullOrUndefined(fileEntry, "FileEntry should be defined.");
    assertNotNullOrUndefined(
        storageDevice.storageUnitId, "Storage Unit should be defined.");

    chrome.imageWriterPrivate.writeFromFile(
        storageDevice.storageUnitId,
        fileEntry,
        startWriteCallback);
  }

  function startWriteCallback() {
    started = true;
  }

  function writeProgressCallback(progressInfo) {
    currentProgress = progressInfo.percentComplete;
    currentStage = progressInfo.stage;
  }

  function writeCompleteCallback() {
    chrome.test.assertTrue(started, "Complete triggered before being started.");
    chrome.test.assertEq(100, currentProgress);
    chrome.test.succeed("Write completed successfully.");
  }

  function writeErrorCallback(message) {
    chrome.test.fail("An error occurred during writing.");
  }

  chrome.imageWriterPrivate.onWriteProgress.
      addListener(writeProgressCallback);
  chrome.imageWriterPrivate.onWriteComplete.
      addListener(writeCompleteCallback);
  chrome.imageWriterPrivate.onWriteError.
      addListener(writeErrorCallback);

  chrome.fileSystem.chooseEntry(chooseEntryCallback);
}

testWriteFromFile();

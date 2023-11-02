// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaGalleries = chrome.mediaGalleries;
var expectedGalleryEntryLength;

function TestFirstFilesystem(verifyFilesystem) {
  function getMediaFileSystemsList() {
    mediaGalleries.getMediaFileSystems(getMediaFileSystemsCallback);
  }

  function getMediaFileSystemsCallback(results) {
    chrome.test.assertEq(1, results.length);
    verifyFilesystem(results[0]);
  }

  getMediaFileSystemsList();
}

function ReadDirectoryTest() {
  function verifyFilesystem(filesystem) {
    verifyDirectoryEntry(filesystem.root, verify);
  }

  function verify(directoryEntry, entries) {
    chrome.test.assertEq(1, entries.length);
    chrome.test.assertFalse(entries[0].isDirectory);
    chrome.test.assertEq("test.jpg", entries[0].name);
    chrome.test.succeed();
  }

  TestFirstFilesystem(verifyFilesystem);
}

function ReadFileToBytesTest() {
  function verifyFilesystem(filesystem) {
    verifyJPEG(filesystem.root, "test.jpg", expectedGalleryEntryLength,
               chrome.test.succeed);
  }

  TestFirstFilesystem(verifyFilesystem);
}

function GetMediaFileSystemMetadataTest() {
  function verifyFilesystem(filesystem) {
    var metadata = mediaGalleries.getMediaFileSystemMetadata(filesystem);
    checkMetadata(metadata);
    chrome.test.succeed();
  }

  TestFirstFilesystem(verifyFilesystem);
}

chrome.test.getConfig(function(config) {
  customArg = JSON.parse(config.customArg);
  expectedGalleryEntryLength = customArg[0];

  chrome.test.runTests([
    ReadDirectoryTest,
    ReadFileToBytesTest,
    GetMediaFileSystemMetadataTest,
  ]);
})

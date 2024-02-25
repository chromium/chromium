// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test files should be created before running the test extension.
 */

function getVolumeMetadataList() {
  return new Promise(function(resolve, reject) {
    chrome.fileManagerPrivate.getVolumeMetadataList(resolve);
  });
}

function requestFileSystem(volumeId) {
  return new Promise(function(resolve, reject) {
    chrome.fileSystem.requestFileSystem(
        {volumeId: volumeId}, function(fileSystem) {
          if (!fileSystem) {
            reject(new Error('Failed to acquire volume.'));
          }
          resolve(fileSystem);
        });
  });
}

function requestAllFileSystems() {
  return getVolumeMetadataList().then(function(volumes) {
    return Promise.all(volumes.map(function(volume) {
      return requestFileSystem(volume.volumeId);
    }));
  });
}

// Run the tests.
requestAllFileSystems().then(function() {
  function exists(entries, fileName) {
    for (let i = 0; i < entries.length; ++i) {
      if (entries[i].name === fileName) {
        return true;
      }
    }
    return false;
  }
  chrome.test.runTests([
    function testGetRecentFiles() {
      chrome.fileManagerPrivate.getRecentFiles(
          'native_source', '', 30, 'all', false,
          chrome.test.callbackPass(entries => {
            chrome.test.assertTrue(
                exists(entries, 'all-justice.jpg'),
                'all-justice.jpg not found');
            chrome.test.assertTrue(
                exists(entries, 'all-justice.mp3'),
                'all-justice.mp3 not found');
            chrome.test.assertTrue(
                exists(entries, 'all-justice.mp4'),
                'all-justice.mp4 not found');
          }));
    },
    function testGetRecentFilesWithQuery() {
      chrome.fileManagerPrivate.getRecentFiles(
          'native_source', 'jpg', 30, 'all', false,
          chrome.test.callbackPass(entries => {
            chrome.test.assertTrue(
                exists(entries, 'all-justice.jpg'),
                'all-justice.jpg not found');
            chrome.test.assertFalse(
                exists(entries, 'all-justice.mp3'),
                'all-justice.mp3 unexpectedly found');
            chrome.test.assertFalse(
                exists(entries, 'all-justice.mp4'),
                'all-justice.mp4 unexpectedly found');
          }));
    },
    function testGetRecentAudioFiles() {
      chrome.fileManagerPrivate.getRecentFiles(
          'native_source', '', 30, 'audio', false,
          chrome.test.callbackPass(entries => {
            chrome.test.assertFalse(
                exists(entries, 'all-justice.jpg'),
                'all-justice.jpg unexpectedly found');
            chrome.test.assertTrue(
                exists(entries, 'all-justice.mp3'),
                'all-justice.mp3 not found');
            chrome.test.assertFalse(
                exists(entries, 'all-justice.mp4'),
                'all-justice.mp4 unexpectedly found');
          }));
    },
    function testGetRecentImageFiles() {
      chrome.fileManagerPrivate.getRecentFiles(
          'native_source', '', 30, 'image', false,
          chrome.test.callbackPass(entries => {
            chrome.test.assertTrue(
                exists(entries, 'all-justice.jpg'),
                'all-justice.jpg not found');
            chrome.test.assertFalse(
                exists(entries, 'all-justice.mp3'),
                'all-justice.mp3 unexpectedly found');
            chrome.test.assertFalse(
                exists(entries, 'all-justice.mp4'),
                'all-justice.mp4 unexpectedly found');
          }));
    },
    function testGetRecentVideoFiles() {
      chrome.fileManagerPrivate.getRecentFiles(
          'native_source', '', 30, 'video', false,
          chrome.test.callbackPass(entries => {
            chrome.test.assertFalse(
                exists(entries, 'all-justice.jpg'),
                'all-justice.jpg unexpectedly found');
            chrome.test.assertFalse(
                exists(entries, 'all-justice.mp3'),
                'all-justice.mp3 unexpectedly found');
            chrome.test.assertTrue(
                exists(entries, 'all-justice.mp4'),
                'all-justice.mp4 not found');
          }));
    },
    function testGetOlderRecentFiles() {
      chrome.fileManagerPrivate.getRecentFiles(
          'native_source', 'all-justice', 61, 'all', false,
          chrome.test.callbackPass(entries => {
            chrome.test.assertTrue(
                exists(entries, 'all-justice.jpg'),
                'all-justice.jpg not found');
            chrome.test.assertTrue(
                exists(entries, 'all-justice.mp3'),
                'all-justice.mp3 not found');
            chrome.test.assertTrue(
                exists(entries, 'all-justice.mp4'),
                'all-justice.mp4 not found');
            chrome.test.assertTrue(
                exists(entries, 'all-justice.txt'),
                'all-justice.txt not found');
          }));
    },
    function testCheckNonPositiveDeltas() {
      // Asking for negative deltas is like asking for files modified in the
      // future. You should never get any results.
      chrome.fileManagerPrivate.getRecentFiles(
          'native_source', 'all-justice', 0, 'all', false,
          chrome.test.callbackPass(entries => {
            chrome.test.assertEq(0, entries.length);
          }));
      chrome.fileManagerPrivate.getRecentFiles(
          'native_source', 'all-justice', -1, 'all', false,
          chrome.test.callbackPass(entries => {
            chrome.test.assertEq(0, entries.length);
          }));
      const max31BitValue = Math.pow(2, 31) - 1;
      chrome.fileManagerPrivate.getRecentFiles(
          'native_source', 'all-justice', -max31BitValue, 'all', false,
          chrome.test.callbackPass(entries => {
            chrome.test.assertEq(0, entries.length);
          }));
      chrome.fileManagerPrivate.getRecentFiles(
          'native_source', 'all-justice', -max31BitValue - 1, 'all', false,
          chrome.test.callbackPass(entries => {
            chrome.test.assertEq(0, entries.length);
          }));
    }
  ]);
});

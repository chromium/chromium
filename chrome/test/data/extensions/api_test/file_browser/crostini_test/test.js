// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// This api testing extension's ID.  Files referenced as Entry will
// have this as part of their URL.
const TEST_EXTENSION_ID = 'pkplfbidichfdicaijlchgnapepdginl';

/**
 * Get specified entry.
 * @param {string} volumeType volume type for entry.
 * @param {string} path path of entry.
 * @return {!Entry} specified entry.
 */
function getEntry(volumeType, path) {
  return new Promise(resolve => {
    chrome.fileManagerPrivate.getVolumeMetadataList(list => {
      const volume = list.find(v => v.volumeType === volumeType);
      chrome.fileSystem.requestFileSystem({volumeId: volume.volumeId}, fs => {
        fs.root.getDirectory(path, {}, entry => {
          resolve(entry);
        });
      });
    });
  });
}

// Run the tests.
chrome.test.runTests([
  function testMountCrostini() {
    chrome.fileManagerPrivate.mountCrostini(
        chrome.test.callbackPass());
  },
  function testSharePathsWithCrostiniSuccess() {
    getEntry('downloads', 'share_dir').then((entry) => {
      chrome.fileManagerPrivate.sharePathsWithCrostini(
          'termina', [entry], true, chrome.test.callbackPass());
    });
  },
  function testSharePathsWithCrostiniNotDownloads() {
    getEntry('testing', 'test_dir').then((entry) => {
      chrome.fileManagerPrivate.sharePathsWithCrostini(
          'termina', [entry], true,
          chrome.test.callbackFail('Path is not allowed'));
    });
  },
  function testGetCrostiniSharedPaths() {
    const urlPrefix = 'filesystem:chrome-extension://' + TEST_EXTENSION_ID +
        '/external/Downloads-user';
    let observeFirstForSession = false;
    chrome.fileManagerPrivate.getCrostiniSharedPaths(
        observeFirstForSession, 'termina',
        chrome.test.callbackPass(({entries, firstForSession}) => {
          // 2 entries inserted in setup, and 1 successful entry added above.
          chrome.test.assertEq(urlPrefix + '/share_dir', entries[0].toURL());
          chrome.test.assertTrue(entries[0].isDirectory);
          chrome.test.assertEq('/share_dir', entries[0].fullPath);
          chrome.test.assertEq(3, entries.length);
          chrome.test.assertEq(urlPrefix + '/shared1', entries[1].toURL());
          chrome.test.assertTrue(entries[1].isDirectory);
          chrome.test.assertEq('/shared1', entries[1].fullPath);
          chrome.test.assertEq(urlPrefix + '/shared2', entries[2].toURL());
          chrome.test.assertTrue(entries[2].isDirectory);
          chrome.test.assertEq('/shared2', entries[2].fullPath);
          // When observerFirstForSession is false, firstForSession is false.
          chrome.test.assertFalse(firstForSession);
        }));
    // First time observeFirstForSession is set true, firstForSession is true.
    observeFirstForSession = true;
    chrome.fileManagerPrivate.getCrostiniSharedPaths(
        observeFirstForSession, 'termina',
        chrome.test.callbackPass(({entries, firstForSession}) => {
          chrome.test.assertEq(3, entries.length);
          chrome.test.assertTrue(firstForSession);
        }));
    // Subsequent times, firstForSession is false.
    chrome.fileManagerPrivate.getCrostiniSharedPaths(
        observeFirstForSession, 'termina',
        chrome.test.callbackPass(({entries, firstForSession}) => {
          chrome.test.assertEq(3, entries.length);
          chrome.test.assertFalse(firstForSession);
        }));
  },
  function testUnsharePathWithCrostiniSuccess() {
    getEntry('downloads', 'share_dir').then((entry) => {
      chrome.fileManagerPrivate.unsharePathWithCrostini(
          'termina', entry, chrome.test.callbackPass());
    });
  },
]);

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The test file system should be created and populated before running the test
 * extension.
 *
 * The only file used right now is <root>/test_dir/test_file.txt.
 */

// This is a golden value computed using the md5sum command line tool.
var kExpectedHash = 'a3dfffb5a580272fb8986611a9dbd166';

function getTestFilesystem() {
  return new Promise(function(resolve, reject) {
    chrome.fileManagerPrivate.getVolumeMetadataList(
        function(volumeMetadataList) {
          var testVolume = volumeMetadataList.filter(function(volume) {
            return volume.volumeType === 'testing';
          })[0];

          chrome.fileSystem.requestFileSystem(
              {volumeId: testVolume.volumeId},
              function(fileSystem) {
                if (!fileSystem) {
                  reject(new Error('Failed to acquire the testing volume.'));
                }
                resolve(fileSystem);
              });
        });
  });
}

// Run the tests.
getTestFilesystem().then(
    function(fileSystem) {
      chrome.test.runTests([
          // Checks the checksum code using a golden file.
          function testGoldenChecksum() {
            new Promise(
                function(fulfill, reject) {
                  fileSystem.root.getFile(
                      'test_dir/test_file.txt',
                      {create: false},
                      function(entry) {
                        chrome.test.assertTrue(!!entry);
                        fulfill(entry);
                      });
                  })
                .then(
                    function(entry) {
                      chrome.fileManagerPrivate.computeChecksum(
                          entry,
                          chrome.test.callbackPass(function(result) {
                            chrome.test.assertEq(kExpectedHash, result);
                          }));
                    })
                .catch(chrome.test.fail);
          }
      ]);
    });

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Extension apitests for drive search methods.
 * There are three tests functions run:
 * - loadFileSystem() which requests the Drive file system.
 * - driveSearch() which tests chrome.fileManagerPrivate.searchDrive function.
 * - driveMetadataSearch() which tests
 *   chrome.fileManagerPrivate.searchDriveMetadata function.
 *
 * For both search test functions, the test verifies that the file system
 * operations can be performed on the returned result entries. For file entries
 * by trying FileEntry.file function and for directory entries by trying
 * to run the deirectories directory reader.
 */

/**
 * Verifies that a file snapshot can be created for the entry.
 * On failure, the current test is ended with failure.
 * On success, |successCallback| is called.
 *
 * @param {FileEntry} entry File entry to be tested.
 * @param {function()} successCallback The function called when the file entry
 *     is successfully verified.
 */
function verifyFile(entry, successCallback) {
  chrome.test.assertFalse(!entry.file, 'Entry has no file method.');
  entry.file(successCallback,
             function() {
               chrome.test.fail('Error reading result file.');
              });
}

/**
 * Verifies that a directory reader can be created and used for the entry.
 * On failure, the current test is ended with failure.
 * On success, |successCallback| is called.
 *
 * @param {DirectoryEntry} entry Directory entry to be tested.
 * @param {function()} successCallback The function called when the dir entry
 *     is successfully verified.
 */
function verifyDirectory(entry, successCallback) {
  chrome.test.assertFalse(!entry.createReader,
                          'Entry has no createReader method.');
  var reader = entry.createReader();

  reader.readEntries(successCallback, function (error) {
    chrome.test.fail('Error reading directory: ' + error.name);
  });
}

/**
 * Returns the fuction that should be used to verify the entry whose type is
 * specified by |type|.
 *
 * @param {string} type The entry type. Can be either 'file' or 'dir'.
 * @return {function(Entry, successCallback)}
 */
function getEntryVerifier(type) {
  if (type == 'file')
    return verifyFile;

  if (type == 'dir')
    return verifyDirectory;

  return null;
}

chrome.test.runTests([
  // Loads filesystem that contains drive mount point.
  function loadFileSystem() {
    chrome.fileSystem.requestFileSystem(
      {volumeId: 'drive:drive-user'},
      function (fileSystem) {
        chrome.test.assertFalse(!fileSystem, 'Failed to get file system.');
        fileSystem.root.getDirectory('/root/test_dir', {create: false},
            // Also read a non-root directory. This will initiate loading of
            // the full resource metadata. As of now, 'search' only works
            // with the resource metadata fully loaded. crbug.com/181075
            function(entry) {
              var reader = entry.createReader();
              reader.readEntries(
                  chrome.test.succeed,
                  function() {
                    chrome.test.fail('Error reading directory.');
                  });
            },
            function() {
              chrome.test.fail('Unable to get the testing directory.');
            });
      });
  },

  // Tests chrome.fileManagerPrivate.searchDrive method.
  function driveSearch() {
    var query = 'empty';
    var expectedEntries = {
      '/root/test_dir/empty_dir': 'dir',
      '/root/test_dir/empty_file.foo': 'file',
    };

    chrome.fileManagerPrivate.searchDrive(
        {query: query, nextFeed: ''}, chrome.test.callbackPass((response) => {
          const {entries, nextFeed} = response;
          chrome.test.assertFalse(!!nextFeed);
          chrome.test.assertFalse(!entries);

          // The search query should return exactly two results.
          chrome.test.assertEq(2, entries.length);

          var index = -1;
          for (var i = 0; i < entries.length; ++i) {
            var expectedType = expectedEntries[entries[i].fullPath];
            chrome.test.assertTrue(!!expectedType);
            var verifyEntry = getEntryVerifier(expectedType);
            chrome.test.assertFalse(!verifyEntry);
            verifyEntry(entries[i], chrome.test.callbackPass());
          }
        }));
  },

  // Tests chrome.fileManagerPrivate.searchDriveMetadata method.
  function driveMetadataSearch() {
    // The results should be sorted by (lastAccessed, lastModified) pair. The
    // sort should be decending. The comments above each expected result
    // represent their (lastAccessed, lastModified) pair.
    // The API should return 4 results, even though there are more than five
    // matches in the test file system.
    var expectedResults = [
        // (2012-01-02T00:00:01.000Z, 2012-01-02T00:00:0.000Z)
        {path: '/root/test_dir', type: 'dir'},
        // (2012-01-02T00:00:00.000Z, 2012-01-01T00:00:00.005Z)
        {path: '/root/test_dir/test_file.xul', type: 'file'},
        // (2012-01-02T00:00:00.000Z, 2011-04-03T11:11:10.000Z)
        {path: '/root/test_dir/test_file.tiff', type: 'file'},
        // (2012-01-01T11:00:00.000Z, 2012-01-01T10:00:30.00Z)
        {path: '/root/test_dir/test_file.xul.foo', type: 'file'},
    ];

    var query = {
      'query': 'test',
      'types': 'ALL',
      'maxResults': 4
    };

    chrome.fileManagerPrivate.searchDriveMetadata(
        query,
        function(entries) {
          chrome.test.assertFalse(!entries);
          chrome.test.assertEq(expectedResults.length, entries.length);

          function verifySearchResultAt(resultIndex) {
            if (resultIndex == expectedResults.length) {
              chrome.test.succeed();
              return;
            }

            chrome.test.assertFalse(!entries[resultIndex]);
            chrome.test.assertFalse(!entries[resultIndex].entry);
            chrome.test.assertEq(expectedResults[resultIndex].path,
                                 entries[resultIndex].entry.fullPath);

            var verifyEntry =
                getEntryVerifier(expectedResults[resultIndex].type);
            chrome.test.assertFalse(!verifyEntry);

            // The callback will be called only if the entry is successfully
            // verified, otherwise the test function will fail.
            verifyEntry(entries[resultIndex].entry,
                        verifySearchResultAt.bind(null, resultIndex + 1));
          }

          verifySearchResultAt(0);
        });
  }
]);

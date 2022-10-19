// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {catchError, mountTestFileSystem} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';

async function main() {
  await navigator.serviceWorker.ready;
  const fileSystem = await mountTestFileSystem();

  const TEST_DIR = 'empty-directory';

  chrome.test.runTests([
    // Create a directory (not exclusive). Should succeed.
    async function createDirectorySuccessSimple() {
      const entry = await fileSystem.getDirectoryEntry(
          TEST_DIR, {create: true, exclusive: false});

      chrome.test.assertEq(TEST_DIR, entry.name);
      chrome.test.assertTrue(entry.isDirectory);
      chrome.test.succeed();
    },

    // Create a directory (exclusive). Should fail, since the directory already
    // exists.
    async function createDirectoryErrorExists() {
      const error = await catchError(fileSystem.getDirectoryEntry(
          TEST_DIR, {create: true, exclusive: true}));
      chrome.test.assertTrue(!!error, 'Created a directory, but should fail.');
      chrome.test.assertEq('InvalidModificationError', error.name);
      chrome.test.succeed();
    },
  ])
}

main();
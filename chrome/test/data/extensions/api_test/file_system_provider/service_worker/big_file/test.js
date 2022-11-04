// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {mountTestFileSystem, openFile, readTextFromBlob, remoteProvider} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';
// For shared constants.
import {TestFileSystemProvider} from '/_test_resources/api_test/file_system_provider/service_worker/provider.js';

async function main() {
  await navigator.serviceWorker.ready;

  const fileSystem = await mountTestFileSystem();

  chrome.test.runTests([
    async function readBigFileSuccess() {
      const fileEntry = await fileSystem.getFileEntry(
          TestFileSystemProvider.FILE_BIG,
          {create: false},
      );
      const file = await openFile(fileEntry);
      // Read 10 bytes past the max unsigned 32-bit integer offset.
      const offset = 2 ** 32 + 100;
      const text = await readTextFromBlob(file.slice(offset, offset + 10));

      // Check the provider got the read request at the correct offset.
      chrome.test.assertEq(
          offset,
          (await remoteProvider.waitForEvent('onReadFileRequested')).offset);
      chrome.test.assertEq('AAAAAAAAAA', text);
      chrome.test.succeed();
    },
  ]);
}

main();

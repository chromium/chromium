// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {catchError, mountTestFileSystem, promisifyWithLastError, remoteProvider} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';
// For shared constants.
import {TestFileSystemProvider} from '/_test_resources/api_test/file_system_provider/service_worker/provider.js';

async function main() {
  await navigator.serviceWorker.ready;

  const fileSystem = await mountTestFileSystem();
  const fileName = TestFileSystemProvider.FILE_READ_SUCCESS;

  chrome.test.runTests([
    // Executes an existing action.
    async function executeActionSuccess() {
      const testActionId = TestFileSystemProvider.ACTION_ID;

      const fileEntry =
          await fileSystem.getFileEntry(fileName, {create: false});
      await promisifyWithLastError(
          chrome.fileManagerPrivate.executeCustomAction, [fileEntry],
          testActionId);

      const {fileSystemId, entryPaths, actionId} =
          await remoteProvider.waitForEvent('onExecuteActionRequested');
      chrome.test.assertEq(TestFileSystemProvider.FILESYSTEM_ID, fileSystemId);
      chrome.test.assertEq(1, entryPaths.length);
      chrome.test.assertEq(`/${fileName}`, entryPaths[0]);
      chrome.test.assertEq(testActionId, actionId);
      chrome.test.succeed();
    },

    // Tries to execute a non-existing action.
    async function executeNonExistingActionFailure() {
      const testActionId = 'unknown-action-id';

      const fileEntry =
          await fileSystem.getFileEntry(fileName, {create: false});
      const error = await catchError(promisifyWithLastError(
          chrome.fileManagerPrivate.executeCustomAction, [fileEntry],
          testActionId));

      chrome.test.assertTrue(!!error, 'Expected executing the action to fail.');
      chrome.test.assertEq('Failed to execute the action.', error.message);
      const {fileSystemId, entryPaths, actionId} =
          await remoteProvider.waitForEvent('onExecuteActionRequested');
      chrome.test.assertEq(TestFileSystemProvider.FILESYSTEM_ID, fileSystemId);
      chrome.test.assertEq(1, entryPaths.length);
      chrome.test.assertEq(`/${fileName}`, entryPaths[0]);
      chrome.test.assertEq(testActionId, actionId);
      chrome.test.succeed();
    }
  ]);
}

main();

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {mountTestFileSystem, promisifyWithLastError} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';
import {TestFileSystemProvider} from '/_test_resources/api_test/file_system_provider/service_worker/provider.js';

/** @param {!Array<!Entry>} entries */
async function getCustomActions(entries) {
  return promisifyWithLastError(
      chrome.fileManagerPrivate.getCustomActions, entries);
}

async function main() {
  await navigator.serviceWorker.ready;

  const fileSystem = await mountTestFileSystem();
  const dirWithActions = TestFileSystemProvider.DIR_WITH_ACTIONS;
  // Actions of dirWithActions.
  const dirWithActionsActions = TestFileSystemProvider.ACTIONS;
  const dirWithNoActions = TestFileSystemProvider.DIR_WITH_NO_ACTIONS;

  chrome.test.runTests([
    // Get actions for a directory with actions.
    async function getActionsSuccess() {
      const dirEntry =
          await fileSystem.getDirectoryEntry(dirWithActions, {create: false});

      const actions = await getCustomActions([dirEntry]);

      chrome.test.assertEq(2, actions.length);
      chrome.test.assertEq(dirWithActionsActions[0].id, actions[0].id);
      chrome.test.assertFalse(!!actions[0].title);
      chrome.test.assertEq(dirWithActionsActions[1].id, actions[1].id);
      chrome.test.assertEq(dirWithActionsActions[1].title, actions[1].title);
      chrome.test.succeed();
    },

    // Get actions for a directory with no actions.
    async function getNoActionsSuccess() {
      const dirEntry =
          await fileSystem.getDirectoryEntry(dirWithNoActions, {create: false});

      const actions = await getCustomActions([dirEntry]);

      chrome.test.assertEq(0, actions.length);
      chrome.test.succeed();
    },

    // Get actions for multiple entries.
    async function getNoActionsMultipleSuccess() {
      const dirEntry =
          await fileSystem.getDirectoryEntry(dirWithActions, {create: false});
      const dirEntry2 =
          await fileSystem.getDirectoryEntry(dirWithNoActions, {create: false});

      const actions = await getCustomActions([dirEntry, dirEntry2]);

      chrome.test.assertEq(0, actions.length);
      chrome.test.succeed();
    },
  ]);
}

main();
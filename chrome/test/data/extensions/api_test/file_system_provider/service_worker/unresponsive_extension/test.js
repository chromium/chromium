// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {catchError, getVolumeInfo, promisifyWithLastError, ProviderProxy} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';
import {TestFileSystemProvider} from '/_test_resources/api_test/file_system_provider/service_worker/provider.js';

async function main() {
  const volumeInfo = await getVolumeInfo(TestFileSystemProvider.FILESYSTEM_ID);
  // fileSystemProvider implementation is hosted in a companion extension (under
  // ./provider) to capture the cases where the FSP extension's UI is shown or
  // not: if the tests are run in the same extension that hosts the FSP, the
  // window will always be open as the actual test code is run in a foreground
  // page.
  const remoteProvider = new ProviderProxy('gfnblenhaahcnmfdbebgincjohfkbnch');

  let tabId = -1;

  chrome.test.runTests([
    // Verify that if no window nor tab is opened, then the request will let
    // users abort the operation via notification shown by |RequestManager|.
    async function unresponsiveWithoutUI() {
      await remoteProvider.setConfig('onConfigureRequestedDelayMs', 1000);
      const error = await catchError(promisifyWithLastError(
          chrome.fileManagerPrivate.configureVolume, volumeInfo.volumeId));

      chrome.test.assertTrue(!!error, 'Expected configureVolume to fail.');
      chrome.test.assertEq('Failed to complete configuration.', error.message);
      chrome.test.succeed();
    },

    // Verify that if a tab is opened, then the request will not invoke
    // a notification.
    async function unresponsiveWithTab() {
      tabId = await remoteProvider.openTab('provider/stub.html');

      await promisifyWithLastError(
          chrome.fileManagerPrivate.configureVolume, volumeInfo.volumeId);

      chrome.test.succeed();
    },

    // Verify that if a window is opened, then the request will not invoke
    // a notification shown by |RequestManager|.
    async function unresponsiveWithWindow() {
      await remoteProvider.closeTab(tabId);
      await remoteProvider.openWindow('provider/stub.html');

      await promisifyWithLastError(
          chrome.fileManagerPrivate.configureVolume, volumeInfo.volumeId);

      chrome.test.succeed();
    },
  ]);
}

main();

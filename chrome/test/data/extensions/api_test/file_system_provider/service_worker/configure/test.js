// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {mountTestFileSystem, promisifyWithLastError, remoteProvider} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';
// For shared constants.
import {TestFileSystemProvider} from '/_test_resources/api_test/file_system_provider/service_worker/provider.js';


async function main() {
  await navigator.serviceWorker.ready;

  const fileSystem = await mountTestFileSystem();

  chrome.test.runTests([
    // Verify that the configuration flag is propagated properly.
    async function configureConfigurable() {
      try {
        let providers = await promisifyWithLastError(
            chrome.fileManagerPrivate.getProviders);
        // Filter out native providers.
        providers = providers.filter(
            provider => provider.providerId.length == 0 ||
                provider.providerId[0] != '@');
        chrome.test.assertEq(providers.length, 1);
        // For extension based providers, provider id is the same as
        // extension id.
        chrome.test.assertEq(chrome.runtime.id, providers[0].providerId);
        chrome.test.assertEq(
            chrome.runtime.getManifest().name, providers[0].name);
        chrome.test.assertTrue(providers[0].configurable);
        chrome.test.assertFalse(providers[0].multipleMounts);
        chrome.test.assertEq('device', providers[0].source);
        await new Promise(
            resolve => chrome.fileManagerPrivate.configureVolume(
                fileSystem.volumeInfo.volumeId, resolve));
        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Verify that chrome.fileManager.configureVolume is well wired
    // to onConfigureRequested().
    async function configureSuccess() {
      try {
        await promisifyWithLastError(
            chrome.fileManagerPrivate.configureVolume,
            fileSystem.volumeInfo.volumeId);
        await remoteProvider.waitForEvent('onConfigureRequested');
        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Verify that a failure is propagated properly.
    async function configureFailure() {
      try {
        // Next call to configure should return error.
        await remoteProvider.setConfig(
            'onConfigureRequestedError',
            chrome.fileSystemProvider.ProviderError.FAILED,
        );
        try {
          await promisifyWithLastError(
              chrome.fileManagerPrivate.configureVolume,
              fileSystem.volumeInfo.volumeId);
          chrome.test.fail('Configuration should have failed.');
        } catch (e) {
          chrome.test.assertEq('Failed to complete configuration.', e.message);
          chrome.test.succeed();
        }
      } catch (e) {
        chrome.test.fail(e);
      }
    }
  ]);
}

main();

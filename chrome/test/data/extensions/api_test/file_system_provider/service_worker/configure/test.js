// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {catchError, mountTestFileSystem, promisifyWithLastError, remoteProvider} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';

async function main() {
  await navigator.serviceWorker.ready;

  const fileSystem = await mountTestFileSystem();

  chrome.test.runTests([
    // Verify that the configuration flag is propagated properly.
    async function configureConfigurable() {
      let providers =
          await promisifyWithLastError(chrome.fileManagerPrivate.getProviders);

      // Filter out native providers.
      providers = providers.filter(
          provider =>
              provider.providerId.length == 0 || provider.providerId[0] != '@');
      chrome.test.assertEq(providers.length, 1);
      // For extension based providers, provider id is the same as
      // extension id.
      chrome.test.assertEq(chrome.runtime.id, providers[0].providerId);
      chrome.test.assertEq(
          chrome.runtime.getManifest().name, providers[0].name);
      chrome.test.assertTrue(providers[0].configurable);
      chrome.test.assertFalse(providers[0].multipleMounts);
      chrome.test.assertEq('device', providers[0].source);

      await promisifyWithLastError(
          chrome.fileManagerPrivate.configureVolume,
          fileSystem.volumeInfo.volumeId);

      chrome.test.succeed();
    },

    // Verify that chrome.fileManager.configureVolume is well wired
    // to onConfigureRequested().
    async function configureSuccess() {
      await promisifyWithLastError(
          chrome.fileManagerPrivate.configureVolume,
          fileSystem.volumeInfo.volumeId);

      await remoteProvider.waitForEvent('onConfigureRequested');
      chrome.test.succeed();
    },

    // Verify that a failure is propagated properly.
    async function configureFailure() {
      // Next call to configure should return error.
      await remoteProvider.setConfig(
          'onConfigureRequestedError',
          chrome.fileSystemProvider.ProviderError.FAILED,
      );

      const error = await catchError(promisifyWithLastError(
          chrome.fileManagerPrivate.configureVolume,
          fileSystem.volumeInfo.volumeId));

      chrome.test.assertTrue(!!error, 'Configuration should have failed.');
      chrome.test.assertEq('Failed to complete configuration.', error.message);
      chrome.test.succeed();
    }
  ]);
}

main();

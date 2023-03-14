// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FileSystemSiteListElement, RawFileSystemGrant, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

// clang-format on
suite('FileSystemSettings_EnablePersistentPermissions', function() {
  let testElement: FileSystemSiteListElement;
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  suiteSetup(function() {
    CrSettingsPrefs.setInitialized();

    loadTimeData.overrideValues({
      showPersistentPermissions: true,
    });
  });

  suiteTeardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  // Initialize the file-system-site-list element.
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    testElement = document.createElement('file-system-site-list');
    document.body.appendChild(testElement);
  });

  teardown(function() {
    testElement.remove();
    Router.getInstance().resetRouteForTesting();
  });

  function navigateToFileSystemSettingsPage() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_FILE_SYSTEM_WRITE);
  }

  test('FileSystemSiteListPopulated', async function() {
    const origin: string = 'https://a.com/';
    const filePath1: string = 'a/b';
    const filePath2: string = 'a/b/c';
    const filePath3: string = 'e/f';
    const directoryFilePath1: string = 'g/h/';
    const directoryFilePath2: string = 'i/';

    const TEST_FILE_SYSTEM_FILE_WRITE_GRANT1: RawFileSystemGrant = {
      origin: origin,
      filePath: filePath1,
      isWritable: true,
      isDirectory: false,
    };
    const TEST_FILE_SYSTEM_FILE_WRITE_GRANT2: RawFileSystemGrant = {
      origin: origin,
      filePath: filePath2,
      isWritable: true,
      isDirectory: false,
    };
    const TEST_FILE_SYSTEM_FILE_READ_GRANT: RawFileSystemGrant = {
      origin: origin,
      filePath: filePath3,
      isWritable: false,
      isDirectory: false,
    };
    const TEST_FILE_SYSTEM_DIRECTORY_READ_GRANT: RawFileSystemGrant = {
      origin: origin,
      filePath: directoryFilePath1,
      isWritable: false,
      isDirectory: true,
    };
    const TEST_FILE_SYSTEM_DIRECTORY_WRITE_GRANT: RawFileSystemGrant = {
      origin: origin,
      filePath: directoryFilePath2,
      isWritable: true,
      isDirectory: true,
    };

    browserProxy.setFileSystemGrants([{
      origin: origin,
      directoryReadGrants: [TEST_FILE_SYSTEM_DIRECTORY_READ_GRANT],
      directoryWriteGrants: [TEST_FILE_SYSTEM_DIRECTORY_WRITE_GRANT],
      fileReadGrants: [TEST_FILE_SYSTEM_FILE_READ_GRANT],
      fileWriteGrants: [
        TEST_FILE_SYSTEM_FILE_WRITE_GRANT1,
        TEST_FILE_SYSTEM_FILE_WRITE_GRANT2,
      ],
    }]);

    // File paths listed in the order that they are displayed on the UI:
    // (For all grants listed in the ordering below, are in the order they
    // are listed in `setFileSystemGrants` above).
    // File write grants -> directory write grants -> file read grants ->
    // directory read grants.
    const allFilePathsInDisplayOrder: string[] = [
      filePath1,
      filePath2,
      directoryFilePath2,
      filePath3,
      directoryFilePath1,
    ];

    navigateToFileSystemSettingsPage();
    await browserProxy.whenCalled('getFileSystemGrants');
    flush();

    // Ensure that the list container element is populated.
    // The number of h2 elements displayed on the page equals the number of
    // origins with allowed permission grants.
    const fileSystemOriginsWithAllowedGrants =
        testElement.shadowRoot!.querySelectorAll('h2');
    assertEquals(1, fileSystemOriginsWithAllowedGrants.length);

    // The number of elements with the `display-name` class attribute
    // equals the number of file paths associated with an
    // allowed permission grant for a given origin.
    const fileSystemAllowedGrants =
        testElement.shadowRoot!.querySelectorAll('div.display-name');
    assertEquals(
        allFilePathsInDisplayOrder.length, fileSystemAllowedGrants.length);

    // The values of the data displayed on the page are as expected.
    for (let i = 0; i < fileSystemAllowedGrants.length; i++) {
      assertEquals(
          allFilePathsInDisplayOrder[i],
          fileSystemAllowedGrants[i]!.textContent);
    }
  });
});
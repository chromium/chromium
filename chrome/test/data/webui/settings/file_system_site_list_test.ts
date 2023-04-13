// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

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

  // Initialize the file-system-site-list element.
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    testElement = document.createElement('file-system-site-list');
    document.body.appendChild(testElement);
  });

  function navigateToFileSystemSettingsPage() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_FILE_SYSTEM_WRITE);
  }

  test('FileSystemSiteListPopulated', async function() {
    const origin: string = 'https://a.com/';
    const origin2: string = 'https://b.com/';
    const filePath1: string = 'a/b';
    const filePath2: string = 'a/b/c';
    const directoryFilePath1: string = 'g/h/';
    const directoryFilePath2: string = 'i/';

    const TEST_FILE_SYSTEM_DIRECTORY_WRITE_GRANT: RawFileSystemGrant = {
      origin: origin,
      filePath: directoryFilePath2,
      isWritable: true,
      isDirectory: true,
    };
    const TEST_FILE_SYSTEM_DIRECTORY_READ_GRANT: RawFileSystemGrant = {
      origin: origin,
      filePath: directoryFilePath1,
      isWritable: false,
      isDirectory: true,
    };
    const TEST_FILE_SYSTEM_FILE_READ_GRANT: RawFileSystemGrant = {
      origin: origin2,
      filePath: filePath2,
      isWritable: false,
      isDirectory: false,
    };
    const TEST_FILE_SYSTEM_FILE_WRITE_GRANT: RawFileSystemGrant = {
      origin: origin2,
      filePath: filePath1,
      isWritable: true,
      isDirectory: false,
    };

    browserProxy.setFileSystemGrants([
      {
        origin: origin,
        directoryReadGrants: [TEST_FILE_SYSTEM_DIRECTORY_READ_GRANT],
        directoryWriteGrants: [TEST_FILE_SYSTEM_DIRECTORY_WRITE_GRANT],
        fileReadGrants: [],
        fileWriteGrants: [],
      },
      {
        origin: origin2,
        directoryReadGrants: [],
        directoryWriteGrants: [],
        fileReadGrants: [TEST_FILE_SYSTEM_FILE_READ_GRANT],
        fileWriteGrants: [TEST_FILE_SYSTEM_FILE_WRITE_GRANT],
      },
    ]);

    navigateToFileSystemSettingsPage();
    await browserProxy.whenCalled('getFileSystemGrants');
    flush();

    // Ensure that the list container element is populated.
    const fileSystemSiteEntries =
        testElement.shadowRoot!.querySelectorAll('file-system-site-entry');
    assertEquals(2, fileSystemSiteEntries.length);
  });
});

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {FileSystemSiteListElement, FileSystemGrant, OriginFileSystemGrants} from 'chrome://settings/lazy_load.js';
import {SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

// clang-format on
suite('FileSystemSettings_EnablePersistentPermissions', function() {
  let testElement: FileSystemSiteListElement;
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  const origin1: string = 'https://a.com/';
  const origin2: string = 'https://b.com/';
  const filePath: string = 'a/b';
  const directoryFilePath: string = 'g/h/';

  const TEST_FILE_SYSTEM_DIRECTORY_READ_GRANT: FileSystemGrant = {
    filePath: directoryFilePath,
    displayName: directoryFilePath,
    isDirectory: true,
  };

  const TEST_FILE_SYSTEM_FILE_WRITE_GRANT: FileSystemGrant = {
    filePath: filePath,
    displayName: filePath,
    isDirectory: false,
  };

  const FILE_SYSTEM_GRANTS: OriginFileSystemGrants[] = [
    {
      origin: origin1,
      viewGrants: [
        TEST_FILE_SYSTEM_DIRECTORY_READ_GRANT,
      ],
      editGrants: [],
    },
    {
      origin: origin2,
      viewGrants: [],
      editGrants: [TEST_FILE_SYSTEM_FILE_WRITE_GRANT],
    },
  ];

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

  test('FileSystemSiteList_IsPopulated', async function() {
    browserProxy.setFileSystemGrants(FILE_SYSTEM_GRANTS);
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_FILE_SYSTEM_WRITE);
    await browserProxy.whenCalled('getFileSystemGrants');
    flush();
    const fileSystemSiteEntries =
        testElement.shadowRoot!.querySelectorAll('file-system-site-entry');
    assertEquals(fileSystemSiteEntries.length, 2);
  });
});

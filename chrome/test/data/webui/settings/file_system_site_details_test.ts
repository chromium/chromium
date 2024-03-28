// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {FileSystemGrant, OriginFileSystemGrants} from 'chrome://settings/lazy_load.js';
import {FileSystemSiteDetailsElement, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

// clang-format on

suite('FileSystemSettings_EnablePersistentPermissions_SiteDetails', function() {
  let testElement: FileSystemSiteDetailsElement;
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  const origin: string = 'https://a.com/';
  const filePath: string = 'a/b';
  const directoryFilePath: string = 'g/h/';

  const TEST_FILE_SYSTEM_FILE_WRITE_GRANT: FileSystemGrant = {
    filePath: filePath,
    displayName: filePath,
    isDirectory: false,
  };
  const TEST_FILE_SYSTEM_DIRECTORY_READ_GRANT: FileSystemGrant = {
    filePath: directoryFilePath,
    displayName: directoryFilePath,
    isDirectory: true,
  };
  const TEST_FILE_SYSTEM_GRANTS_PER_ORIGIN: OriginFileSystemGrants = {
    origin: origin,
    viewGrants: [
      TEST_FILE_SYSTEM_DIRECTORY_READ_GRANT,
    ],
    editGrants: [
      TEST_FILE_SYSTEM_FILE_WRITE_GRANT,
    ],
  };
  const FILE_SYSTEM_GRANTS: OriginFileSystemGrants[] =
      [TEST_FILE_SYSTEM_GRANTS_PER_ORIGIN];
  const FILE_SYSTEM_GRANTS_EMPTY: OriginFileSystemGrants[] = [];

  suiteSetup(function() {
    CrSettingsPrefs.setInitialized();

    loadTimeData.overrideValues({
      showPersistentPermissions: true,
    });
  });

  // Initialize the file-system-site-details element.
  setup(async function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    browserProxy.setFileSystemGrants(FILE_SYSTEM_GRANTS);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = new FileSystemSiteDetailsElement();
    document.body.appendChild(testElement);
    const params = new URLSearchParams();
    params.append('site', origin);
    Router.getInstance().navigateTo(
        routes.SITE_SETTINGS_FILE_SYSTEM_WRITE_DETAILS, params);
    await browserProxy.whenCalled('getFileSystemGrants');
    flush();
  });

  test('FileSystemSiteDetails_FileSystemSiteEntryItemsPopulated', function() {
    const fileSystemSiteDetailsElements =
        testElement.shadowRoot!.querySelectorAll('file-system-site-entry-item');
    assertEquals(fileSystemSiteDetailsElements.length, 2);
  });

  test('FileSystemSiteDetails_NoGrants', async function() {
    // Simulate an origin that previously had permission grants, and now has
    // none due to a change in chooser permissions being updated.
    browserProxy.setFileSystemGrants(FILE_SYSTEM_GRANTS_EMPTY);
    webUIListenerCallback(
        'contentSettingChooserPermissionChanged', 'file-system-write');
    await browserProxy.whenCalled('getFileSystemGrants');
    flush();
    assertEquals(
        Router.getInstance().getCurrentRoute(),
        routes.SITE_SETTINGS_FILE_SYSTEM_WRITE);
  });
});

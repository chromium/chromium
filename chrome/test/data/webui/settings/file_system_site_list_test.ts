// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FileSystemSiteListElement, RawFileSystemGrant, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

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

  test('FileSystemSiteListTests', async function() {
    const origin1: string = 'https://a.com/';
    const origin2: string = 'https://b.com/';
    const filePath1: string = 'a/b';
    const directoryFilePath1: string = 'g/h/';

    const TEST_FILE_SYSTEM_DIRECTORY_READ_GRANT: RawFileSystemGrant = {
      origin: origin1,
      filePath: directoryFilePath1,
      isWritable: false,
      isDirectory: true,
    };
    const TEST_FILE_SYSTEM_FILE_WRITE_GRANT: RawFileSystemGrant = {
      origin: origin2,
      filePath: filePath1,
      isWritable: true,
      isDirectory: false,
    };

    browserProxy.setFileSystemGrants([
      {
        origin: origin1,
        directoryReadGrants: [TEST_FILE_SYSTEM_DIRECTORY_READ_GRANT],
        directoryWriteGrants: [],
        fileReadGrants: [],
        fileWriteGrants: [],
      },
      {
        origin: origin2,
        directoryReadGrants: [],
        directoryWriteGrants: [],
        fileReadGrants: [],
        fileWriteGrants: [TEST_FILE_SYSTEM_FILE_WRITE_GRANT],
      },
    ]);

    Router.getInstance().navigateTo(routes.SITE_SETTINGS_FILE_SYSTEM_WRITE);
    await browserProxy.whenCalled('getFileSystemGrants');
    flush();

    // Ensure that the list container element is populated.
    const fileSystemSiteEntries =
        testElement.shadowRoot!.querySelectorAll('file-system-site-entry');
    assertEquals(2, fileSystemSiteEntries.length);

    // Removing the last remaining grant for a given origin results in the
    // corresponding `file-system-site-entry` element's removal from the UI.
    const removeGrantButton =
        fileSystemSiteEntries[0]!.shadowRoot!
            .querySelectorAll('file-system-site-entry-item')[0]!.shadowRoot!
            .querySelector<HTMLElement>('#removeGrant');
    assertTrue(!!removeGrantButton);
    removeGrantButton.click();
    const [origin, filePath] =
        await browserProxy.whenCalled('revokeFileSystemGrant');
    assertEquals(origin1, origin);
    assertEquals(directoryFilePath1, filePath);
    assertEquals(1, browserProxy.getCallCount('revokeFileSystemGrant'));
  });
});

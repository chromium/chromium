// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {FileSystemGrant, OriginFileSystemGrants} from 'chrome://settings/lazy_load.js';
import {FileSystemSiteEntryElement, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

// clang-format on
suite('FileSystemSettings_EnablePersistentPermissions', function() {
  let testElement: FileSystemSiteEntryElement;
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;
  const kTestOrigin: string = 'https://a.com/';

  suiteSetup(function() {
    CrSettingsPrefs.setInitialized();

    loadTimeData.overrideValues({
      showPersistentPermissions: true,
    });
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    testElement = new FileSystemSiteEntryElement();
    document.body.appendChild(testElement);

    // Initialize the file-system-site-list.
    const filePath: string = 'a/b';
    const TEST_FILE_SYSTEM_FILE_WRITE_GRANT: FileSystemGrant = {
      filePath: filePath,
      displayName: filePath,
      isDirectory: false,
    };
    const FILE_SYSTEM_GRANTS: OriginFileSystemGrants = {
      origin: kTestOrigin,
      viewGrants: [],
      editGrants: [TEST_FILE_SYSTEM_FILE_WRITE_GRANT],
    };
    testElement.grantsPerOrigin = FILE_SYSTEM_GRANTS;
    flush();
  });

  test('FileSystemSiteListEntries_RevokeAllGrants', async function() {
    const removeGrantsButton =
        testElement.shadowRoot!.querySelector<HTMLElement>('#removeGrants');
    assertTrue(!!removeGrantsButton);

    const whenFired = eventToPromise('revoke-grants', testElement);
    removeGrantsButton.click();
    const permissionRemovedEvent = await whenFired;
    const {origin} = permissionRemovedEvent.detail;
    assertEquals(origin, kTestOrigin);
  });

  test('FileSystemSiteListEntries_NavigateToSiteDetails', function() {
    const navigateToSiteDetailsForOriginButton =
        testElement.shadowRoot!.querySelector<HTMLElement>(
            '#fileSystemSiteDetails');
    assertTrue(!!navigateToSiteDetailsForOriginButton);
    navigateToSiteDetailsForOriginButton.click();
    assertEquals(
        Router.getInstance().getCurrentRoute().path,
        routes.SITE_SETTINGS_FILE_SYSTEM_WRITE_DETAILS.path);
    assertEquals(
        Router.getInstance().getQueryParameters().get('site'), kTestOrigin);
  });
});

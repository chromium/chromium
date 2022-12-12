// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {ContentSettingsTypes, SettingsUnusedSitePermissionsElement, SiteSettingsPermissionsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';

import {TestSiteSettingsPermissionsBrowserProxy} from './test_site_settings_permissions_browser_proxy.js';

// clang-format on

suite('CrSettingsUnusedSitePermissionsTest', function() {
  let browserProxy: TestSiteSettingsPermissionsBrowserProxy;

  let testElement: SettingsUnusedSitePermissionsElement;

  const permissions = [
    ContentSettingsTypes.GEOLOCATION,
    ContentSettingsTypes.MIC,
    ContentSettingsTypes.CAMERA,
    ContentSettingsTypes.NOTIFICATIONS,
  ];

  const mockData = [1, 2, 3, 4].map(i => ({
                                      origin: `https://www.example${i}.com:443`,
                                      permissions: permissions.slice(0, i),
                                    }));

  setup(async function() {
    browserProxy = new TestSiteSettingsPermissionsBrowserProxy();
    browserProxy.setUnusedSitePermissions(mockData);
    SiteSettingsPermissionsBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-unused-site-permissions');
    document.body.appendChild(testElement);
    await browserProxy.whenCalled('getRevokedUnusedSitePermissionsList');
    flush();
  });

  test('Unused Site Permission strings', function() {
    const entries =
        testElement.shadowRoot!.querySelectorAll('.site-list .site-entry');
    assertEquals(4, entries.length);

    // Check that the text describing the permissions is correct.
    assertEquals(
        mockData[0]!.origin,
        entries[0]!.querySelector('.site-representation')!.textContent!.trim());
    assertEquals(
        'Removed location',
        entries[0]!.querySelector('.secondary')!.textContent!.trim());

    assertEquals(
        mockData[1]!.origin,
        entries[1]!.querySelector('.site-representation')!.textContent!.trim());
    assertEquals(
        'Removed location, microphone',
        entries[1]!.querySelector('.secondary')!.textContent!.trim());

    assertEquals(
        mockData[2]!.origin,
        entries[2]!.querySelector('.site-representation')!.textContent!.trim());
    assertEquals(
        'Removed location, microphone, camera',
        entries[2]!.querySelector('.secondary')!.textContent!.trim());

    assertEquals(
        mockData[3]!.origin,
        entries[3]!.querySelector('.site-representation')!.textContent!.trim());
    assertEquals(
        'Removed location, microphone, and 2 more',
        entries[3]!.querySelector('.secondary')!.textContent!.trim());
  });

  test('Collapsible List', function() {
    const expandButton =
        testElement.shadowRoot!.querySelector('cr-expand-button');
    assertTrue(!!expandButton);

    const unusedSitePermissionList =
        testElement.shadowRoot!.querySelector('iron-collapse');
    assertTrue(!!unusedSitePermissionList);

    // Button and list start out expanded.
    assertTrue(expandButton.expanded);
    assertTrue(unusedSitePermissionList.opened);

    // User collapses the list.
    expandButton.click();
    flush();

    // Button and list are collapsed.
    assertFalse(expandButton.expanded);
    assertFalse(unusedSitePermissionList.opened);

    // User expands the list.
    expandButton.click();
    flush();

    // Button and list are expanded.
    assertTrue(expandButton.expanded);
    assertTrue(unusedSitePermissionList.opened);
  });
});

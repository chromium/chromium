// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {SettingsUnusedSitePermissionsElement, SiteSettingsPermissionsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';

import {TestSiteSettingsPermissionsBrowserProxy} from './test_site_settings_permissions_browser_proxy.js';

// clang-format on

suite('CrSettingsUnusedSitePermissionsTest', function() {
  let browserProxy: TestSiteSettingsPermissionsBrowserProxy;

  let testElement: SettingsUnusedSitePermissionsElement;

  const origin1 = 'https://www.example1.com:443';
  const permissions1 = ['location', 'notification'];
  const origin2 = 'https://www.example2.com:443';
  const permissions2 = ['microphone', 'camera'];

  const mockData = [
    {
      origin: origin1,
      permissions: permissions1,
    },
    {
      origin: origin2,
      permissions: permissions2,
    },
  ];

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
    assertEquals(2, entries.length);

    // Check that the text describing the permissions is correct.
    assertEquals(
        origin1,
        entries[0]!.querySelector('.site-representation')!.textContent!.trim());
    assertEquals(
        origin2,
        entries[1]!.querySelector('.site-representation')!.textContent!.trim());
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

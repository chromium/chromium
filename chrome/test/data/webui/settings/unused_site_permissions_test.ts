// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {ContentSettingsTypes, SettingsUnusedSitePermissionsElement, SiteSettingsPermissionsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';

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

  /* Asserts for each row whether or not it is animating. */
  function assertAnimation(expectedAnimation: boolean[]) {
    const rows = getSiteList();

    assertEquals(
        rows.length, expectedAnimation.length,
        'Provided ' + expectedAnimation.length +
            ' expectations but there are ' + rows.length + ' rows');
    for (const [i, row] of rows.entries()) {
      assertEquals(
          expectedAnimation[i]!, row!.classList.contains('removed'),
          'Expectation not met for row #' + i);
    }
  }

  /** Assert visibility and content of the undo toast. */
  function assertToast(shouldBeOpen: boolean, expectedText?: string) {
    const undoToast = testElement.shadowRoot!.querySelector('cr-toast')!;
    if (!shouldBeOpen) {
      assertFalse(undoToast.open);
      return;
    }
    assertTrue(undoToast.open);
    const actualText = undoToast.querySelector('div')!.textContent!.trim();
    assertEquals(expectedText, actualText);
  }

  function clickGotIt() {
    const button = testElement.shadowRoot!.querySelector(
                       '.bulk-action-button') as HTMLElement;
    button.click();
  }

  function getSiteList() {
    return testElement.shadowRoot!.querySelectorAll('.site-list .site-entry');
  }

  async function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-unused-site-permissions');
    testElement.setModelUpdateDelayMsForTesting(0);
    document.body.appendChild(testElement);
    await browserProxy.whenCalled('getRevokedUnusedSitePermissionsList');
    flush();
  }

  setup(async function() {
    browserProxy = new TestSiteSettingsPermissionsBrowserProxy();
    browserProxy.setUnusedSitePermissions(mockData);
    SiteSettingsPermissionsBrowserProxyImpl.setInstance(browserProxy);
    await createPage();
  });

  test('Unused Site Permission strings', function() {
    const siteList = getSiteList();
    assertEquals(4, siteList.length);

    // Check that the text describing the permissions is correct.
    assertEquals(
        mockData[0]!.origin,
        siteList[0]!.querySelector(
                        '.site-representation')!.textContent!.trim());
    assertEquals(
        'Removed location',
        siteList[0]!.querySelector('.secondary')!.textContent!.trim());

    assertEquals(
        mockData[1]!.origin,
        siteList[1]!.querySelector(
                        '.site-representation')!.textContent!.trim());
    assertEquals(
        'Removed location, microphone',
        siteList[1]!.querySelector('.secondary')!.textContent!.trim());

    assertEquals(
        mockData[2]!.origin,
        siteList[2]!.querySelector(
                        '.site-representation')!.textContent!.trim());
    assertEquals(
        'Removed location, microphone, camera',
        siteList[2]!.querySelector('.secondary')!.textContent!.trim());

    assertEquals(
        mockData[3]!.origin,
        siteList[3]!.querySelector(
                        '.site-representation')!.textContent!.trim());
    assertEquals(
        'Removed location, microphone, and 2 more',
        siteList[3]!.querySelector('.secondary')!.textContent!.trim());
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

  test('Allow Again Click', async function() {
    const siteList = getSiteList();

    assertEquals(siteList.length, 4);
    assertAnimation([false, false, false, false]);
    assertToast(false);

    siteList[0]!.querySelector('cr-icon-button')!.click();

    assertAnimation([true, false, false, false]);
    // Ensure the browser proxy call is done.
    const expectedOrigin =
        siteList[0]!.querySelector('.site-representation')!.textContent!.trim();
    const [unusedSitePermissions] =
        await browserProxy.whenCalled('allowPermissionsAgainForUnusedSite');
    assertEquals(unusedSitePermissions.origin, expectedOrigin);
    assertDeepEquals(
        unusedSitePermissions.permissions, mockData[0]!.permissions);
    // Ensure the toast text is correct.
    const expectedToastText = testElement.i18n(
        'safetyCheckUnusedSitePermissionsToastLabel', expectedOrigin);
    assertToast(true, expectedToastText);
  });

  test('Got It Click', async function() {
    clickGotIt();
    await flushTasks();

    // Ensure the browser proxy call is done.
    const [unusedSitePermissionsList] = await browserProxy.whenCalled(
        'acknowledgeRevokedUnusedSitePermissionsList');
    // |unusedSitePermissionsList| has the additional property |visible|, so
    // assertDeepEquals doesn't work to compare it with |mockData|.
    assertEquals(unusedSitePermissionsList.length, mockData.length);
    for (let i = 0; i < unusedSitePermissionsList.length; ++i) {
      assertEquals(unusedSitePermissionsList[i].origin, mockData[i]!.origin);
      assertDeepEquals(
          unusedSitePermissionsList[i].permissions, mockData[i]!.permissions);
    }
  });

  test('Got It Toast Strings', async function() {
    // Check plural version of the string.
    assertToast(false);
    clickGotIt();
    await flushTasks();
    const expectedPluralToastText =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckUnusedSitePermissionsToastBulkLabel', mockData.length);
    assertToast(true, expectedPluralToastText);

    // Check singular version of the string.
    const oneElementMockData = mockData.slice(0, 1);
    browserProxy.setUnusedSitePermissions(oneElementMockData);
    await createPage();
    assertToast(false);
    clickGotIt();
    await flushTasks();
    const expectedSingularToastText =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckUnusedSitePermissionsToastBulkLabel', 1);
    assertToast(true, expectedSingularToastText);
  });
});

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-site-permissions. */
import 'chrome://extensions/extensions.js';

import type {ExtensionsSitePermissionsElement} from 'chrome://extensions/extensions.js';
import {navigation, Page, Service} from 'chrome://extensions/extensions.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestService} from './test_service.js';
import {testVisible} from './test_util.js';

suite('SitePermissions', function() {
  let element: ExtensionsSitePermissionsElement;
  let delegate: TestService;
  let listenerId: number = 0;

  const userSiteSettings: chrome.developerPrivate.UserSiteSettings = {
    permittedSites: ['http://google.com', 'http://example.com'],
    restrictedSites: [],
  };

  setup(function() {
    loadTimeData.overrideValues({'enableUserPermittedSites': true});

    delegate = new TestService();
    delegate.userSiteSettings = userSiteSettings;
    Service.setInstance(delegate);

    setupElement();
  });

  function setupElement() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('extensions-site-permissions');
    element.delegate = delegate;
    element.enableEnhancedSiteControls = true;
    document.body.appendChild(element);
  }

  teardown(function() {
    if (listenerId !== 0) {
      assertTrue(navigation.removeListener(listenerId));
      listenerId = 0;
    }
  });

  test('user site settings are present', async function() {
    await delegate.whenCalled('getUserSiteSettings');
    await microtasksFinished();

    const sitePermissionLists =
        element!.shadowRoot!.querySelectorAll<HTMLElement>(
            'site-permissions-list');
    assertEquals(2, sitePermissionLists.length);
    const permittedSites = sitePermissionLists[0]!;
    const restrictedSites = sitePermissionLists[1]!;

    // Test that there are 2 sites visible for permitted sites, and the
    // '#no-sites' messages is not visible.
    testVisible(permittedSites, '#no-sites', false);
    assertEquals(
        2, permittedSites.shadowRoot!.querySelectorAll('.site-row').length);

    // Test that the '#no-sites' message is visible for restricted sites.
    testVisible(restrictedSites!, '#no-sites', true);
    assertEquals(
        0, restrictedSites!.shadowRoot!.querySelectorAll('.site-row').length);
  });

  test('user site settings update when event is fired', async function() {
    await delegate.whenCalled('getUserSiteSettings');
    await microtasksFinished();

    // Send an event which updates the list of permitted and restricted sites.
    delegate.userSiteSettingsChangedTarget.callListeners(
        {permittedSites: [], restrictedSites: ['http://example.com']});
    await microtasksFinished();

    const sitePermissionLists =
        element!.shadowRoot!.querySelectorAll<HTMLElement>(
            'site-permissions-list');
    assertEquals(2, sitePermissionLists.length);
    const permittedSites = sitePermissionLists[0]!;
    const restrictedSites = sitePermissionLists[1]!;

    // Test that the '#no-sites' message is visible for permitted sites.
    testVisible(permittedSites, '#no-sites', true);
    assertEquals(
        0, permittedSites.shadowRoot!.querySelectorAll('.site-row').length);

    // Test that there is one site visible for restricted sites, and the
    // '#no-sites' messages is not visible.
    testVisible(restrictedSites!, '#no-sites', false);
    assertEquals(
        1, restrictedSites!.shadowRoot!.querySelectorAll('.site-row').length);
  });

  test('clicking a link navigates to the all sites page', async () => {
    let currentPage = null;
    listenerId = navigation.addListener(newPage => {
      currentPage = newPage;
    });

    await microtasksFinished();
    const allSitesLink = element.$.allSitesLink;
    assertTrue(!!allSitesLink);
    assertTrue(isVisible(allSitesLink));

    allSitesLink.click();
    await microtasksFinished();

    assertDeepEquals(currentPage, {page: Page.SITE_PERMISSIONS_ALL_SITES});
  });

  test(
      'permitted sites not visible when enableUserPermittedSites flag is false',
      async () => {
        loadTimeData.overrideValues({'enableUserPermittedSites': false});

        // set up the element again to capture the updated value of
        // enableUserPermittedSites.
        setupElement();

        await microtasksFinished();
        const sitePermissionLists =
            element!.shadowRoot!.querySelectorAll<HTMLElement>(
                'site-permissions-list');

        // Only the list of user restricted sites should be visible.
        assertEquals(1, sitePermissionLists.length);
      });
});

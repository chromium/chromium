// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-site-permissions-all-sites. */
import 'chrome://extensions/extensions.js';

import type {ExtensionsSitePermissionsBySiteElement} from 'chrome://extensions/extensions.js';
import {navigation, Page} from 'chrome://extensions/extensions.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestService} from './test_service.js';

suite('SitePermissionsBySite', function() {
  let element: ExtensionsSitePermissionsBySiteElement;
  let delegate: TestService;
  let listenerId: number = 0;

  const siteGroups: chrome.developerPrivate.SiteGroup[] = [
    {
      etldPlusOne: 'google.ca',
      numExtensions: 0,
      sites: [
        {
          siteSet: chrome.developerPrivate.SiteSet.USER_PERMITTED,
          numExtensions: 0,
          site: 'images.google.ca',
        },
        {
          siteSet: chrome.developerPrivate.SiteSet.USER_RESTRICTED,
          numExtensions: 0,
          site: 'google.ca',
        },
      ],
    },
    {
      etldPlusOne: 'example.com',
      numExtensions: 0,
      sites: [{
        siteSet: chrome.developerPrivate.SiteSet.USER_PERMITTED,
        numExtensions: 0,
        site: 'example.com',
      }],
    },
  ];

  setup(function() {
    delegate = new TestService();
    delegate.siteGroups = siteGroups;

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('extensions-site-permissions-by-site');
    element.delegate = delegate;
    document.body.appendChild(element);
  });

  teardown(function() {
    if (listenerId !== 0) {
      assertTrue(navigation.removeListener(listenerId));
      listenerId = 0;
    }
  });

  test(
      'clicking close button navigates back to site permissions page',
      async () => {
        let currentPage = null;
        listenerId = navigation.addListener(newPage => {
          currentPage = newPage;
        });

        const closeButton = element.$.closeButton;
        assertTrue(!!closeButton);
        assertTrue(isVisible(closeButton));

        closeButton.click();
        await microtasksFinished();

        assertDeepEquals(currentPage, {page: Page.SITE_PERMISSIONS});
      });

  test('extension and user sites are present', async function() {
    await delegate.whenCalled('getUserAndExtensionSitesByEtld');
    await microtasksFinished();

    const sitePermissionGroups =
        element.shadowRoot!.querySelectorAll<HTMLElement>(
            'site-permissions-site-group');
    assertEquals(2, sitePermissionGroups.length);
  });

  test(
      'extension and user sites update when userSiteSettingsChanged is fired',
      async function() {
        await delegate.whenCalled('getUserAndExtensionSitesByEtld');
        await microtasksFinished();
        delegate.resetResolver('getUserAndExtensionSitesByEtld');
        delegate.siteGroups = [{
          etldPlusOne: 'random.com',
          numExtensions: 0,
          sites: [{
            siteSet: chrome.developerPrivate.SiteSet.USER_RESTRICTED,
            numExtensions: 0,
            site: 'www.random.com',
          }],
        }];

        delegate.userSiteSettingsChangedTarget.callListeners(
            {permittedSites: [], restrictedSites: ['www.random.com']});
        await delegate.whenCalled('getUserAndExtensionSitesByEtld');
        await microtasksFinished();

        const sitePermissionGroups =
            element.shadowRoot!.querySelectorAll<HTMLElement>(
                'site-permissions-site-group');
        assertEquals(1, sitePermissionGroups.length);
      });

  test(
      'extension and user sites update when itemStateChanged is fired',
      async function() {
        await delegate.whenCalled('getUserAndExtensionSitesByEtld');
        await microtasksFinished();
        delegate.resetResolver('getUserAndExtensionSitesByEtld');
        delegate.siteGroups = [{
          etldPlusOne: 'random.com',
          numExtensions: 1,
          sites: [{
            siteSet: chrome.developerPrivate.SiteSet.EXTENSION_SPECIFIED,
            numExtensions: 1,
            site: 'www.random.com',
          }],
        }];

        // Fire a fake event, which should trigger another call to
        // getUserAndExtensionSitesByEtld.
        delegate.itemStateChangedTarget.callListeners({
          event_type: chrome.developerPrivate.EventType.UNINSTALLED,
          item_id: '',
        });
        await delegate.whenCalled('getUserAndExtensionSitesByEtld');
        await microtasksFinished();

        const sitePermissionGroups =
            element.shadowRoot!.querySelectorAll<HTMLElement>(
                'site-permissions-site-group');
        assertEquals(1, sitePermissionGroups.length);
      });
});

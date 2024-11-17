// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for site-permissions-list. */
import 'chrome://extensions/extensions.js';

import type {ExtensionsSitePermissionsListElement} from 'chrome://extensions/extensions.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestService} from './test_service.js';

suite('SitePermissionsList', function() {
  let element: ExtensionsSitePermissionsListElement;
  let delegate: TestService;

  setup(function() {
    delegate = new TestService();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('site-permissions-list');
    element.delegate = delegate;
    element.siteSet = chrome.developerPrivate.SiteSet.USER_RESTRICTED;
    element.sites = [];

    document.body.appendChild(element);
  });

  test('clicking add opens dialog', async () => {
    await microtasksFinished();
    const addSiteButton = element.$.addSite;
    assertTrue(!!addSiteButton);
    assertTrue(isVisible(addSiteButton));

    addSiteButton.click();
    await microtasksFinished();

    const dialog =
        element.shadowRoot!.querySelector('site-permissions-edit-url-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.$.dialog.open);
  });

  test('removing sites through action menu', async function() {
    element.sites = ['https://google.com', 'http://www.example.com'];
    await microtasksFinished();

    const openEditSites =
        element!.shadowRoot!.querySelectorAll<HTMLElement>('.icon-more-vert');
    assertEquals(2, openEditSites.length);
    openEditSites[1]!.click();

    const actionMenu = element.$.siteActionMenu;
    assertTrue(!!actionMenu);
    assertTrue(actionMenu.open);

    const remove = actionMenu.querySelector<HTMLElement>('#remove-site');
    assertTrue(!!remove);

    remove.click();
    const [siteSet, hosts] =
        await delegate.whenCalled('removeUserSpecifiedSites');
    assertEquals(chrome.developerPrivate.SiteSet.USER_RESTRICTED, siteSet);
    assertDeepEquals(['http://www.example.com'], hosts);
    assertFalse(actionMenu.open);
  });

  test(
      'clicking "edit site url" through action menu opens a dialog',
      async function() {
        element.sites = ['https://google.com', 'http://www.example.com'];
        await microtasksFinished();

        const openEditSites =
            element!.shadowRoot!.querySelectorAll<HTMLElement>(
                '.icon-more-vert');
        assertEquals(2, openEditSites.length);
        openEditSites[1]!.click();

        const actionMenu = element.$.siteActionMenu;
        assertTrue(!!actionMenu);
        assertTrue(actionMenu.open);

        const actionMenuEditUrl =
            actionMenu.querySelector<HTMLElement>('#edit-site-url');
        assertTrue(!!actionMenuEditUrl);

        actionMenuEditUrl.click();
        await microtasksFinished();
        assertFalse(actionMenu.open);

        const dialog = element.shadowRoot!.querySelector(
            'site-permissions-edit-url-dialog');
        assertTrue(!!dialog);
        assertTrue(dialog.$.dialog.open);
        assertEquals('http://www.example.com', dialog.siteToEdit);
      });

  test(
      'clicking "edit site permissions" through action menu opens a dialog',
      async function() {
        element.sites = ['https://google.com', 'http://www.example.com'];
        await microtasksFinished();

        const openEditSites =
            element!.shadowRoot!.querySelectorAll<HTMLElement>(
                '.icon-more-vert');
        assertEquals(2, openEditSites.length);
        openEditSites[1]!.click();

        const actionMenu = element.$.siteActionMenu;
        assertTrue(!!actionMenu);
        assertTrue(actionMenu.open);

        const actionMenuEditPermissions =
            actionMenu.querySelector<HTMLElement>('#edit-site-permissions');
        assertTrue(!!actionMenuEditPermissions);

        actionMenuEditPermissions.click();
        await microtasksFinished();
        assertFalse(actionMenu.open);

        const dialog = element.shadowRoot!.querySelector(
            'site-permissions-edit-permissions-dialog');
        assertTrue(!!dialog);
        assertTrue(dialog.$.dialog.open);
        assertEquals('http://www.example.com', dialog.site);
        assertEquals(element.siteSet, dialog.originalSiteSet);
      });
});

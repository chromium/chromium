// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for site-permissions-list. */
import 'chrome://extensions/extensions.js';

import {ExtensionsSitePermissionsListElement} from 'chrome://extensions/extensions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestService} from './test_service.js';

suite('SitePermissionsList', function() {
  let element: ExtensionsSitePermissionsListElement;
  let delegate: TestService;

  setup(function() {
    delegate = new TestService();

    document.body.innerHTML = '';
    element = document.createElement('site-permissions-list');
    element.delegate = delegate;
    element.siteSet = chrome.developerPrivate.UserSiteSet.RESTRICTED;
    element.sites = [];

    document.body.appendChild(element);
  });

  test('clicking add opens dialog', function() {
    flush();
    const addSiteButton = element.$.addSite;
    assertTrue(!!addSiteButton);
    assertTrue(isVisible(addSiteButton));

    addSiteButton.click();
    flush();

    const dialog =
        element.shadowRoot!.querySelector('site-permissions-add-site-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.$.dialog.open);
  });

  test('removing sites through action menu', async function() {
    element.sites = ['https://google.com', 'http://www.example.com'];
    flush();

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
    const [siteSet, host] =
        await delegate.whenCalled('removeUserSpecifiedSite');
    assertEquals(chrome.developerPrivate.UserSiteSet.RESTRICTED, siteSet);
    assertEquals('http://www.example.com', host);
    assertFalse(actionMenu.open);
  });
});

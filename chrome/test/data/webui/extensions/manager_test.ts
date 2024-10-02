// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';
import type {ExtensionsManagerElement} from 'chrome://extensions/extensions.js';
import {navigation, Page, Service} from 'chrome://extensions/extensions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

interface ChromeEventWithDispatch extends
    ChromeEvent<(data: chrome.developerPrivate.EventData) => void> {
  dispatch<E>(obj: E): void;
}

suite('ExtensionManagerTest', function() {
  let manager: ExtensionsManagerElement;

  function assertViewActive(tagName: string) {
    assertTrue(!!manager.$.viewManager.querySelector(`${tagName}.active`));
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    manager = document.createElement('extensions-manager');
    document.body.appendChild(manager);

    // Wait for the first view to be active before starting tests, if one is
    // not active already. Sometimes, on Mac with native HTML imports
    // disabled, no views are active at this point.
    return manager.$.viewManager.querySelector('.active') ?
        Promise.resolve() :
        eventToPromise('view-enter-start', manager);
  });

  function getExtensions(): chrome.developerPrivate.ExtensionInfo[] {
    return manager.shadowRoot!.querySelector(
                                  'extensions-item-list')!.extensions;
  }

  function getApps(): chrome.developerPrivate.ExtensionInfo[] {
    return manager.shadowRoot!.querySelector('extensions-item-list')!.apps;
  }

  test('ItemListVisibility', function() {
    function getExtensionByName(name: string):
        chrome.developerPrivate.ExtensionInfo|null {
      return getExtensions().find(el => el.name === name) || null;
    }

    const extension = getExtensionByName('My extension 1');
    assertTrue(!!extension);

    const list = manager.$['items-list'];

    function listHasItemWithName(name: string): boolean {
      return list.extensions.some(el => el.name === name);
    }

    assertTrue(listHasItemWithName('My extension 1'));

    const target = Service.getInstance().getItemStateChangedTarget() as
        ChromeEventWithDispatch;

    target.dispatch<chrome.developerPrivate.EventData>({
      event_type: chrome.developerPrivate.EventType.UNINSTALLED,
      item_id: extension.id,
    });
    flush();
    assertFalse(listHasItemWithName('My extension 1'));

    target.dispatch<chrome.developerPrivate.EventData>({
      event_type: chrome.developerPrivate.EventType.INSTALLED,
      item_id: extension.id,
      extensionInfo: extension,
    });
    flush();
    assertTrue(listHasItemWithName('My extension 1'));
  });

  test('SplitItems', function() {
    function hasExtensionWithName(name: string): boolean {
      return getExtensions().some(el => el.name === name);
    }

    function hasAppWithName(name: string): boolean {
      return getApps().some(el => el.name === name);
    }

    // Test that we properly split up the items into two sections.
    assertTrue(hasExtensionWithName('My extension 1'));
    assertTrue(hasAppWithName('Platform App Test: minimal platform app'));
    assertTrue(hasAppWithName('hosted_app'));
    assertTrue(hasAppWithName('Packaged App Test'));
  });

  test('ChangePages', function() {
    // We start on the item list.
    manager.shadowRoot!.querySelector(
                           'extensions-sidebar')!.$.sectionsExtensions.click();
    flush();
    assertViewActive('extensions-item-list');

    // Switch: item list -> keyboard shortcuts.
    manager.shadowRoot!.querySelector(
                           'extensions-sidebar')!.$.sectionsShortcuts.click();
    flush();
    assertViewActive('extensions-keyboard-shortcuts');

    // Switch: item list -> detail view.
    const item =
        manager.$['items-list'].shadowRoot!.querySelector('extensions-item');
    assertTrue(!!item);
    const detailsButton =
        item.shadowRoot!.querySelector<HTMLElement>('#detailsButton');
    assertTrue(!!detailsButton);
    detailsButton.click();
    flush();
    assertViewActive('extensions-detail-view');

    // Switch: detail view -> keyboard shortcuts.
    manager.shadowRoot!.querySelector(
                           'extensions-sidebar')!.$.sectionsShortcuts.click();
    flush();
    assertViewActive('extensions-keyboard-shortcuts');

    // We get back on the item list.
    manager.shadowRoot!.querySelector(
                           'extensions-sidebar')!.$.sectionsExtensions.click();
    flush();
    assertViewActive('extensions-item-list');
  });

  test('CloseDrawerOnNarrowModeExit', async function() {
    manager.$.toolbar.narrow = true;
    const toolbar = manager.$.toolbar.$.toolbar;
    await microtasksFinished();
    toolbar.shadowRoot!.querySelector<HTMLElement>('#menuButton')!.click();

    await eventToPromise('cr-drawer-opened', manager);
    const drawer = manager.shadowRoot!.querySelector('cr-drawer');
    assertTrue(!!drawer);

    manager.$.toolbar.narrow = false;
    await eventToPromise('close', drawer);
  });

  test('PageTitleUpdate', function() {
    assertEquals('Extensions', document.title);

    // Open details view with a valid ID.
    navigation.navigateTo(
        {page: Page.DETAILS, extensionId: 'ldnnhddmnhbkjipkidpdiheffobcpfmf'});
    flush();
    assertEquals('Extensions - My extension 1', document.title);

    // Navigate back to the list view and check the page title.
    navigation.navigateTo({page: Page.LIST});
    flush();
    assertEquals('Extensions', document.title);
  });

  test('NavigateToSitePermissionsFail', function() {
    assertFalse(manager.enableEnhancedSiteControls);

    // Try to open the site permissions page.
    navigation.navigateTo({page: Page.SITE_PERMISSIONS});
    flush();

    // Should be re-routed to the main page with enableEnhancedSiteControls
    // set to false.
    assertViewActive('extensions-item-list');

    // Try to open the site permissions all-sites page.
    navigation.navigateTo({page: Page.SITE_PERMISSIONS_ALL_SITES});
    flush();

    // Should be re-routed to the main page.
    assertViewActive('extensions-item-list');
  });

  test('NavigateToSitePermissionsSuccess', function() {
    // Set the enableEnhancedSiteControls flag to true.
    manager.enableEnhancedSiteControls = true;
    flush();

    // Try to open the site permissions page. The navigation should succeed
    // with enableEnhancedSiteControls set to true.
    navigation.navigateTo({page: Page.SITE_PERMISSIONS});
    flush();
    assertViewActive('extensions-site-permissions');

    // Try to open the site permissions all-sites page. The navigation
    // should succeed.
    navigation.navigateTo({page: Page.SITE_PERMISSIONS_ALL_SITES});
    flush();
    assertViewActive('extensions-site-permissions-by-site');
  });
});

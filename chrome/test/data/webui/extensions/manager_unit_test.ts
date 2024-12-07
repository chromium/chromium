// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for extension-manager unit tests. Unlike
 * extension_manager_test.js, these tests are not interacting with the real
 * chrome.developerPrivate API.
 */

import type {ExtensionsManagerElement} from 'chrome://extensions/extensions.js';
import {navigation, Page, Service} from 'chrome://extensions/extensions.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestService} from './test_service.js';
import {createExtensionInfo} from './test_util.js';

suite('ExtensionManagerUnitTest', function() {
  let manager: ExtensionsManagerElement;
  let service: TestService;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    service = new TestService();
    Service.setInstance(service);

    manager = document.createElement('extensions-manager');
    document.body.appendChild(manager);

    // Wait until Manager calls fetches data and initializes itself before
    // making any assertions.
    return Promise.all([
      service.whenCalled('getExtensionsInfo'),
      service.whenCalled('getProfileConfiguration'),
    ]);
  });

  /**
   * Trigger an event that indicates that an extension was installed.
   */
  function simulateExtensionInstall(
      info: chrome.developerPrivate.ExtensionInfo) {
    service.itemStateChangedTarget.callListeners({
      event_type: chrome.developerPrivate.EventType.INSTALLED,
      extensionInfo: info,
    });
  }

  function getExtensions(): chrome.developerPrivate.ExtensionInfo[] {
    return manager.$['items-list']!.extensions;
  }

  function getExtension(index: number): chrome.developerPrivate.ExtensionInfo {
    const extensions = getExtensions();
    const extension = extensions[index];
    assertTrue(!!extension);
    return extension;
  }

  // Test that newly added items are inserted in the correct order.
  test('ItemOrder', async () => {
    assertEquals(0, getExtensions().length);

    const alphaFromStore = createExtensionInfo({
      location: chrome.developerPrivate.Location.FROM_STORE,
      name: 'Alpha',
      id: 'a'.repeat(32),
    });
    simulateExtensionInstall(alphaFromStore);
    await microtasksFinished();
    assertEquals(1, getExtensions().length);
    assertEquals(alphaFromStore.id, getExtension(0).id);

    // Unpacked extensions come first.
    const betaUnpacked = createExtensionInfo({
      location: chrome.developerPrivate.Location.UNPACKED,
      name: 'Beta',
      id: 'b'.repeat(32),
    });
    simulateExtensionInstall(betaUnpacked);
    await microtasksFinished();
    assertEquals(2, getExtensions().length);
    assertEquals(betaUnpacked.id, getExtension(0).id);
    assertEquals(alphaFromStore.id, getExtension(1).id);

    // Extensions from the same location are sorted by name.
    const gammaUnpacked = createExtensionInfo({
      location: chrome.developerPrivate.Location.UNPACKED,
      name: 'Gamma',
      id: 'c'.repeat(32),
    });
    simulateExtensionInstall(gammaUnpacked);
    await microtasksFinished();
    assertEquals(3, getExtensions().length);
    assertEquals(betaUnpacked.id, getExtension(0).id);
    assertEquals(gammaUnpacked.id, getExtension(1).id);
    assertEquals(alphaFromStore.id, getExtension(2).id);

    // The name-sort should be case-insensitive, and should fall back on
    // id.
    const aaFromStore = createExtensionInfo({
      location: chrome.developerPrivate.Location.FROM_STORE,
      name: 'AA',
      id: 'd'.repeat(32),
    });
    simulateExtensionInstall(aaFromStore);
    await microtasksFinished();
    const AaFromStore = createExtensionInfo({
      location: chrome.developerPrivate.Location.FROM_STORE,
      name: 'Aa',
      id: 'e'.repeat(32),
    });
    simulateExtensionInstall(AaFromStore);
    await microtasksFinished();
    const aAFromStore = createExtensionInfo({
      location: chrome.developerPrivate.Location.FROM_STORE,
      name: 'aA',
      id: 'f'.repeat(32),
    });
    simulateExtensionInstall(aAFromStore);
    await microtasksFinished();

    assertEquals(6, getExtensions().length);
    assertEquals(betaUnpacked.id, getExtension(0).id);
    assertEquals(gammaUnpacked.id, getExtension(1).id);
    assertEquals(aaFromStore.id, getExtension(2).id);
    assertEquals(AaFromStore.id, getExtension(3).id);
    assertEquals(aAFromStore.id, getExtension(4).id);
    assertEquals(alphaFromStore.id, getExtension(5).id);
  });

  test('SetItemData', async () => {
    const description = 'description';

    const extension = createExtensionInfo({description: description});
    simulateExtensionInstall(extension);
    await microtasksFinished();

    // The detail view is not present until navigation.
    assertFalse(!!manager.shadowRoot!.querySelector('extensions-detail-view'));
    navigation.navigateTo({page: Page.DETAILS, extensionId: extension.id});
    await microtasksFinished();
    const detailsView =
        manager.shadowRoot!.querySelector('extensions-detail-view');
    assertTrue(!!detailsView);  // View should now be present.
    assertEquals(extension.id, detailsView.data.id);
    assertEquals(description, detailsView.data.description);
    const content =
        detailsView.shadowRoot!.querySelector('.section .section-content');
    assertTrue(!!content);
    assertEquals(description, content.textContent!.trim());
  });

  test(
      'UpdateItemData', async () => {
        const oldDescription = 'old description';
        const newDescription = 'new description';

        const extension = createExtensionInfo({description: oldDescription});
        simulateExtensionInstall(extension);
        const secondExtension = createExtensionInfo({
          description: 'irrelevant',
          id: 'b'.repeat(32),
        });
        simulateExtensionInstall(secondExtension);

        navigation.navigateTo({page: Page.DETAILS, extensionId: extension.id});
        const detailsView =
            manager.shadowRoot!.querySelector('extensions-detail-view');
        assertTrue(!!detailsView);

        const extensionCopy = Object.assign({}, extension);
        extensionCopy.description = newDescription;
        service.itemStateChangedTarget.callListeners({
          event_type: chrome.developerPrivate.EventType.PREFS_CHANGED,
          extensionInfo: extensionCopy,
        });
        await microtasksFinished();

        // Updating a different extension shouldn't have any impact.
        const secondExtensionCopy = Object.assign({}, secondExtension);
        secondExtensionCopy.description = 'something else';
        service.itemStateChangedTarget.callListeners({
          event_type: chrome.developerPrivate.EventType.PREFS_CHANGED,
          extensionInfo: secondExtensionCopy,
        });
        await microtasksFinished();
        assertEquals(extension.id, detailsView.data.id);
        assertEquals(newDescription, detailsView.data.description);

        await microtasksFinished();
        const content =
            detailsView.shadowRoot!.querySelector('.section .section-content');
        assertTrue(!!content);
        assertEquals(newDescription, content.textContent!.trim());
      });

  test('ProfileSettings', async () => {
    assertFalse(manager.inDevMode);

    service.profileStateChangedTarget.callListeners({inDeveloperMode: true});
    assertTrue(manager.inDevMode);

    service.profileStateChangedTarget.callListeners({inDeveloperMode: false});
    assertFalse(manager.inDevMode);

    service.profileStateChangedTarget.callListeners({canLoadUnpacked: true});
    assertTrue(manager.canLoadUnpacked);

    service.profileStateChangedTarget.callListeners({canLoadUnpacked: false});
    assertFalse(manager.canLoadUnpacked);

    service.profileStateChangedTarget.callListeners(
        {isMv2DeprecationNoticeDismissed: true});
    assertTrue(manager.isMv2DeprecationNoticeDismissed);
    await microtasksFinished();
    assertTrue(manager.$['items-list'].isMv2DeprecationNoticeDismissed);

    service.profileStateChangedTarget.callListeners(
        {isMv2DeprecationNoticeDismissed: false});
    assertFalse(manager.isMv2DeprecationNoticeDismissed);
    await microtasksFinished();
    assertFalse(manager.$['items-list'].isMv2DeprecationNoticeDismissed);
  });

  test('Uninstall', async () => {
    assertEquals(0, getExtensions().length);

    const extension = createExtensionInfo({
      location: chrome.developerPrivate.Location.FROM_STORE,
      name: 'Alpha',
      id: 'a'.repeat(32),
    });
    simulateExtensionInstall(extension);
    await microtasksFinished();
    assertEquals(1, getExtensions().length);

    service.itemStateChangedTarget.callListeners({
      event_type: chrome.developerPrivate.EventType.UNINSTALLED,
      // When an extension is uninstalled, only the ID is passed back from
      // C++.
      item_id: extension.id,
    });

    await microtasksFinished();
    assertEquals(0, getExtensions().length);
  });

  // Test that when extensions are uninstalled while on the item list page, the
  // correct element is focused.
  test('UninstallFocus', async function() {
    assertEquals(0, getExtensions().length);

    const extension1 = createExtensionInfo({
      location: chrome.developerPrivate.Location.FROM_STORE,
      name: 'Alpha',
      id: 'a'.repeat(32),
    });

    const extension2 = createExtensionInfo({
      location: chrome.developerPrivate.Location.FROM_STORE,
      name: 'Bravo',
      id: 'b'.repeat(32),
    });

    const extension3 = createExtensionInfo({
      location: chrome.developerPrivate.Location.FROM_STORE,
      name: 'Charlie',
      id: 'c'.repeat(32),
      mustRemainInstalled: true,
    });

    simulateExtensionInstall(extension1);
    simulateExtensionInstall(extension2);
    simulateExtensionInstall(extension3);
    await microtasksFinished();
    assertEquals(3, getExtensions().length);

    const itemList = manager.$['items-list']!;

    service.itemStateChangedTarget.callListeners({
      event_type: chrome.developerPrivate.EventType.UNINSTALLED,
      item_id: extension1.id,
    });

    // After removing `extension1`, focus should go to the remove button of
    // `extension2` which is now the first extension shown.
    await microtasksFinished();
    assertEquals(2, getExtensions().length);
    let button = itemList.getRemoveButton(extension2.id);
    assertTrue(!!button);
    await eventToPromise('focus', button);
    assertEquals(
        getDeepActiveElement(), itemList.getRemoveButton(extension2.id)!);

    service.itemStateChangedTarget.callListeners({
      event_type: chrome.developerPrivate.EventType.UNINSTALLED,
      item_id: extension2.id,
    });

    // Since `extension3` cannot be uninstalled, focus should go to its details
    // button.
    await microtasksFinished();
    assertEquals(1, getExtensions().length);
    button = itemList.getDetailsButton(extension3.id);
    assertTrue(!!button);
    await eventToPromise('focus', button);
    assertEquals(
        getDeepActiveElement(), itemList.getDetailsButton(extension3.id)!);

    // Pretend that `extension3` can be uninstalled to test focus behavior when
    // there are no extensions left.
    service.itemStateChangedTarget.callListeners({
      event_type: chrome.developerPrivate.EventType.UNINSTALLED,
      item_id: extension3.id,
    });

    // Wait for a focus event to be emitted from the toolbar, indicating that
    // the search input has been focused. Without this, there may be a race
    // condition where the search input may not be focused in time for this test
    // to check.
    await eventToPromise('focus', manager.$.toolbar);

    assertEquals(0, getExtensions().length);

    // The search bar should be focused after all extensions have been removed.
    // Tests that the fix for crbug.com/1416324 works by not having the focus be
    // on a deleted element.
    assertTrue(manager.$.toolbar.isSearchFocused());
  });

  function assertViewActive(tagName: string) {
    assertTrue(!!manager.$.viewManager.querySelector(`${tagName}.active`));
  }

  test(
      'UninstallFromDetails', async () => {
        const extension = createExtensionInfo({
          location: chrome.developerPrivate.Location.FROM_STORE,
          name: 'Alpha',
          id: 'a'.repeat(32),
        });
        simulateExtensionInstall(extension);

        navigation.navigateTo({page: Page.DETAILS, extensionId: extension.id});
        await microtasksFinished();
        assertViewActive('extensions-detail-view');

        const whenPopstate = eventToPromise('popstate', window);

        service.itemStateChangedTarget.callListeners({
          event_type: chrome.developerPrivate.EventType.UNINSTALLED,
          // When an extension is uninstalled, only the ID is passed back from
          // C++.
          item_id: extension.id,
        });
        await whenPopstate;
        assertViewActive('extensions-item-list');
      });

  test(
      'ToggleIncognito', async () => {
        assertEquals(0, getExtensions().length);
        const extension = createExtensionInfo({
          location: chrome.developerPrivate.Location.FROM_STORE,
          name: 'Alpha',
          id: 'a'.repeat(32),
        });
        simulateExtensionInstall(extension);
        await microtasksFinished();
        assertEquals(1, getExtensions().length);

        assertEquals(extension, getExtension(0));
        assertTrue(extension.incognitoAccess.isEnabled);
        assertFalse(extension.incognitoAccess.isActive);

        // Simulate granting incognito permission.
        const extensionCopy1 = Object.assign({}, extension);
        extensionCopy1.incognitoAccess.isActive = true;
        service.itemStateChangedTarget.callListeners({
          event_type: chrome.developerPrivate.EventType.LOADED,
          extensionInfo: extensionCopy1,
        });

        await microtasksFinished();
        assertTrue(getExtension(0).incognitoAccess.isActive);

        // Simulate revoking incognito permission.
        const extensionCopy2 = Object.assign({}, extension);
        extensionCopy2.incognitoAccess.isActive = false;
        service.itemStateChangedTarget.callListeners({
          event_type: chrome.developerPrivate.EventType.LOADED,
          extensionInfo: extensionCopy2,
        });
        await microtasksFinished();
        assertFalse(getExtension(0).incognitoAccess.isActive);
      });

  test(
      'EnableAndDisable', async () => {
        const ExtensionState = chrome.developerPrivate.ExtensionState;
        assertEquals(0, getExtensions().length);
        const extension = createExtensionInfo({
          location: chrome.developerPrivate.Location.FROM_STORE,
          name: 'My extension 1',
          id: 'a'.repeat(32),
        });
        simulateExtensionInstall(extension);
        await microtasksFinished();
        assertEquals(1, getExtensions().length);

        assertEquals(extension, getExtension(0));
        assertEquals('My extension 1', extension.name);
        assertEquals(ExtensionState.ENABLED, extension.state);

        // Simulate disabling an extension.
        const extensionCopy1 = Object.assign({}, extension);
        extensionCopy1.state = ExtensionState.DISABLED;
        service.itemStateChangedTarget.callListeners({
          event_type: chrome.developerPrivate.EventType.LOADED,
          extensionInfo: extensionCopy1,
        });
        await microtasksFinished();
        assertEquals(ExtensionState.DISABLED, getExtension(0).state);

        // Simulate re-enabling an extension.
        // Simulate disabling an extension.
        const extensionCopy2 = Object.assign({}, extension);
        extensionCopy2.state = ExtensionState.ENABLED;
        service.itemStateChangedTarget.callListeners({
          event_type: chrome.developerPrivate.EventType.LOADED,
          extensionInfo: extensionCopy2,
        });
        await microtasksFinished();
        assertEquals(ExtensionState.ENABLED, getExtension(0).state);
      });
});

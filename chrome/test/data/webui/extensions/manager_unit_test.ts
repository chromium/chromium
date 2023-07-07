// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for extension-manager unit tests. Unlike
 * extension_manager_test.js, these tests are not interacting with the real
 * chrome.developerPrivate API.
 */

import {ExtensionsManagerElement, navigation, Page, Service} from 'chrome://extensions/extensions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

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
    return manager.shadowRoot!.querySelector(
                                  'extensions-item-list')!.extensions;
  }

  function getExtension(index: number): chrome.developerPrivate.ExtensionInfo {
    const extensions = getExtensions();
    const extension = extensions[index];
    assertTrue(!!extension);
    return extension;
  }

  // Test that newly added items are inserted in the correct order.
  test('ItemOrder', function() {
    assertEquals(0, getExtensions().length);

    const alphaFromStore = createExtensionInfo({
      location: chrome.developerPrivate.Location.FROM_STORE,
      name: 'Alpha',
      id: 'a'.repeat(32),
    });
    simulateExtensionInstall(alphaFromStore);
    assertEquals(1, getExtensions().length);
    assertEquals(alphaFromStore.id, getExtension(0).id);

    // Unpacked extensions come first.
    const betaUnpacked = createExtensionInfo({
      location: chrome.developerPrivate.Location.UNPACKED,
      name: 'Beta',
      id: 'b'.repeat(32),
    });
    simulateExtensionInstall(betaUnpacked);
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
    const AaFromStore = createExtensionInfo({
      location: chrome.developerPrivate.Location.FROM_STORE,
      name: 'Aa',
      id: 'e'.repeat(32),
    });
    simulateExtensionInstall(AaFromStore);
    const aAFromStore = createExtensionInfo({
      location: chrome.developerPrivate.Location.FROM_STORE,
      name: 'aA',
      id: 'f'.repeat(32),
    });
    simulateExtensionInstall(aAFromStore);

    assertEquals(6, getExtensions().length);
    assertEquals(betaUnpacked.id, getExtension(0).id);
    assertEquals(gammaUnpacked.id, getExtension(1).id);
    assertEquals(aaFromStore.id, getExtension(2).id);
    assertEquals(AaFromStore.id, getExtension(3).id);
    assertEquals(aAFromStore.id, getExtension(4).id);
    assertEquals(alphaFromStore.id, getExtension(5).id);
  });

  test('SetItemData', function() {
    const description = 'description';

    const extension = createExtensionInfo({description: description});
    simulateExtensionInstall(extension);

    // The detail view is not present until navigation.
    assertFalse(!!manager.shadowRoot!.querySelector('extensions-detail-view'));
    navigation.navigateTo({page: Page.DETAILS, extensionId: extension.id});
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
      'UpdateItemData', function() {
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

        // Updating a different extension shouldn't have any impact.
        const secondExtensionCopy = Object.assign({}, secondExtension);
        secondExtensionCopy.description = 'something else';
        service.itemStateChangedTarget.callListeners({
          event_type: chrome.developerPrivate.EventType.PREFS_CHANGED,
          extensionInfo: secondExtensionCopy,
        });
        assertEquals(extension.id, detailsView.data.id);
        assertEquals(newDescription, detailsView.data.description);

        const content =
            detailsView.shadowRoot!.querySelector('.section .section-content');
        assertTrue(!!content);
        assertEquals(newDescription, content.textContent!.trim());
      });

  test('ProfileSettings', function() {
    assertFalse(manager.inDevMode);

    service.profileStateChangedTarget.callListeners({inDeveloperMode: true});
    assertTrue(manager.inDevMode);

    service.profileStateChangedTarget.callListeners({inDeveloperMode: false});
    assertFalse(manager.inDevMode);

    service.profileStateChangedTarget.callListeners({canLoadUnpacked: true});
    assertTrue(manager.canLoadUnpacked);

    service.profileStateChangedTarget.callListeners({canLoadUnpacked: false});
    assertFalse(manager.canLoadUnpacked);
  });

  test('Uninstall', function() {
    assertEquals(0, getExtensions().length);

    const extension = createExtensionInfo({
      location: chrome.developerPrivate.Location.FROM_STORE,
      name: 'Alpha',
      id: 'a'.repeat(32),
    });
    simulateExtensionInstall(extension);
    assertEquals(1, getExtensions().length);

    service.itemStateChangedTarget.callListeners({
      event_type: chrome.developerPrivate.EventType.UNINSTALLED,
      // When an extension is uninstalled, only the ID is passed back from
      // C++.
      item_id: extension.id,
    });

    assertEquals(0, getExtensions().length);
  });

  function assertViewActive(tagName: string) {
    assertTrue(!!manager.$.viewManager.querySelector(`${tagName}.active`));
  }

  test(
      'UninstallFromDetails', function(done) {
        const extension = createExtensionInfo({
          location: chrome.developerPrivate.Location.FROM_STORE,
          name: 'Alpha',
          id: 'a'.repeat(32),
        });
        simulateExtensionInstall(extension);

        navigation.navigateTo({page: Page.DETAILS, extensionId: extension.id});
        flush();
        assertViewActive('extensions-detail-view');

        window.addEventListener('popstate', () => {
          assertViewActive('extensions-item-list');
          done();
        });

        service.itemStateChangedTarget.callListeners({
          event_type: chrome.developerPrivate.EventType.UNINSTALLED,
          // When an extension is uninstalled, only the ID is passed back from
          // C++.
          item_id: extension.id,
        });
      });

  test(
      'ToggleIncognito', function() {
        assertEquals(0, getExtensions().length);
        const extension = createExtensionInfo({
          location: chrome.developerPrivate.Location.FROM_STORE,
          name: 'Alpha',
          id: 'a'.repeat(32),
        });
        simulateExtensionInstall(extension);
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

        assertTrue(getExtension(0).incognitoAccess.isActive);

        // Simulate revoking incognito permission.
        const extensionCopy2 = Object.assign({}, extension);
        extensionCopy2.incognitoAccess.isActive = false;
        service.itemStateChangedTarget.callListeners({
          event_type: chrome.developerPrivate.EventType.LOADED,
          extensionInfo: extensionCopy2,
        });
        assertFalse(getExtension(0).incognitoAccess.isActive);
      });

  test(
      'EnableAndDisable', function() {
        const ExtensionState = chrome.developerPrivate.ExtensionState;
        assertEquals(0, getExtensions().length);
        const extension = createExtensionInfo({
          location: chrome.developerPrivate.Location.FROM_STORE,
          name: 'My extension 1',
          id: 'a'.repeat(32),
        });
        simulateExtensionInstall(extension);
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
        assertEquals(ExtensionState.DISABLED, getExtension(0).state);

        // Simulate re-enabling an extension.
        // Simulate disabling an extension.
        const extensionCopy2 = Object.assign({}, extension);
        extensionCopy2.state = ExtensionState.ENABLED;
        service.itemStateChangedTarget.callListeners({
          event_type: chrome.developerPrivate.EventType.LOADED,
          extensionInfo: extensionCopy2,
        });
        assertEquals(ExtensionState.ENABLED, getExtension(0).state);
      });
});

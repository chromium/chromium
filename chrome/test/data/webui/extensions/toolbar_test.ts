// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ExtensionsToolbarElement} from 'chrome://extensions/extensions.js';
import {getToastManager} from 'chrome://extensions/extensions.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestService} from './test_service.js';
import {createExtensionInfo, testVisible} from './test_util.js';

suite('ExtensionToolbarTest', function() {
  let mockDelegate: TestService;
  let toolbar: ExtensionsToolbarElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    toolbar = document.createElement('extensions-toolbar');
    document.body.appendChild(toolbar);
    toolbar.inDevMode = false;
    toolbar.devModeControlledByPolicy = false;
    toolbar.isChildAccount = false;

    mockDelegate = new TestService();
    toolbar.delegate = mockDelegate;

    // The toast manager is normally a child of the <extensions-manager>
    // element, so add it separately for this test.
    const toastManager = document.createElement('cr-toast-manager');
    document.body.appendChild(toastManager);
  });

  test('Layout', async () => {
    const boundTestVisible = testVisible.bind(null, toolbar);
    boundTestVisible('#devMode', true);
    assertEquals(toolbar.$.devMode.disabled, false);
    boundTestVisible('#loadUnpacked', false);
    boundTestVisible('#packExtensions', false);
    boundTestVisible('#updateNow', false);
    toolbar.inDevMode = true;
    await microtasksFinished();

    boundTestVisible('#devMode', true);
    assertEquals(toolbar.$.devMode.disabled, false);
    boundTestVisible('#loadUnpacked', true);
    boundTestVisible('#packExtensions', true);
    boundTestVisible('#updateNow', true);

    toolbar.canLoadUnpacked = false;
    await microtasksFinished();

    boundTestVisible('#devMode', true);
    boundTestVisible('#loadUnpacked', false);
    boundTestVisible('#packExtensions', true);
    boundTestVisible('#updateNow', true);
  });

  test('DevModeToggle', async () => {
    const toggle = toolbar.$.devMode;
    assertFalse(toggle.disabled);

    // Test that the dev-mode toggle is disabled when a policy exists.
    toolbar.devModeControlledByPolicy = true;
    await microtasksFinished();
    assertTrue(toggle.disabled);

    toolbar.devModeControlledByPolicy = false;
    await microtasksFinished();
    assertFalse(toggle.disabled);

    // Test that the dev-mode toggle is disabled for child account users.
    toolbar.isChildAccount = true;
    await microtasksFinished();
    assertTrue(toggle.disabled);
  });

  test('ClickHandlers', async function() {
    toolbar.inDevMode = true;
    await microtasksFinished();
    const toastManager = getToastManager();
    toolbar.$.devMode.click();
    let arg = await mockDelegate.whenCalled('setProfileInDevMode');
    assertFalse(arg);

    mockDelegate.reset();
    toolbar.$.devMode.click();
    arg = await mockDelegate.whenCalled('setProfileInDevMode');
    assertTrue(arg);

    mockDelegate.setLoadUnpackedSuccess(true);
    toolbar.$.loadUnpacked.click();
    await mockDelegate.whenCalled('loadUnpacked');
    assertTrue(toastManager.isToastOpen);

    // Hide toast since it is open for 3000ms in previous Promise.
    toastManager.hide();
    mockDelegate.setLoadUnpackedSuccess(false);
    toolbar.$.loadUnpacked.click();
    await mockDelegate.whenCalled('loadUnpacked');
    assertFalse(toastManager.isToastOpen);
    assertFalse(toastManager.isToastOpen);

    toolbar.$.updateNow.click();
    // Simulate user rapidly clicking update button multiple times.
    toolbar.$.updateNow.click();
    assertTrue(toastManager.isToastOpen);
    await mockDelegate.whenCalled('updateAllExtensions');
    assertEquals(1, mockDelegate.getCallCount('updateAllExtensions'));
    assertFalse(!!toolbar.shadowRoot!.querySelector('extensions-pack-dialog'));
    toolbar.$.packExtensions.click();
    await microtasksFinished();
    const dialog = toolbar.shadowRoot!.querySelector('extensions-pack-dialog');
    assertTrue(!!dialog);
  });

  /** Tests that the update button properly fires the load-error event. */
  test(
      'FailedUpdateFiresLoadError', async function() {
        const item = document.createElement('extensions-item');
        item.data = createExtensionInfo(
            {location: chrome.developerPrivate.Location.UNPACKED});
        item.delegate = mockDelegate;
        document.body.appendChild(item);
        item.inDevMode = true;

        toolbar.inDevMode = true;
        await microtasksFinished();

        const proxyDelegate = new TestService();
        toolbar.delegate = proxyDelegate;

        let firedLoadError = false;
        toolbar.addEventListener('load-error', () => {
          firedLoadError = true;
        }, {once: true});

        function verifyLoadErrorFired(assertCalled: boolean): Promise<void> {
          return new Promise<void>(resolve => {
            setTimeout(() => {
              assertEquals(assertCalled, firedLoadError);
              resolve();
            });
          });
        }

        toolbar.$.devMode.click();
        await microtasksFinished();
        toolbar.$.updateNow.click();
        await proxyDelegate.whenCalled('updateAllExtensions');
        await verifyLoadErrorFired(false);

        proxyDelegate.resetResolver('updateAllExtensions');
        proxyDelegate.setForceReloadItemError(true);
        toolbar.$.updateNow.click();
        await proxyDelegate.whenCalled('updateAllExtensions');
        await verifyLoadErrorFired(true);
      });

  test('NarrowModeShowsMenu', async () => {
    toolbar.narrow = true;
    await microtasksFinished();
    assertTrue(toolbar.$.toolbar.showMenu);

    toolbar.narrow = false;
    await microtasksFinished();
    assertFalse(toolbar.$.toolbar.showMenu);
  });
});

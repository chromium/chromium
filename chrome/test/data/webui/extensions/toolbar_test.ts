// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ExtensionsToolbarElement, getToastManager} from 'chrome://extensions/extensions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
// <if expr="chromeos_ash">
import {eventToPromise} from 'chrome://webui-test/test_util.js';

// </if>

import {TestService} from './test_service.js';
import {createExtensionInfo, testVisible} from './test_util.js';

/** @fileoverview Suite of tests for extension-toolbar. */
const extension_toolbar_tests = {
  suiteName: 'ExtensionToolbarTest',
  TestNames: {
    Layout: 'layout',
    ClickHandlers: 'click handlers',
    DevModeToggle: 'dev mode toggle',
    // <if expr="chromeos_ash">
    KioskMode: 'kiosk mode button',
    // </if>
    FailedUpdateFiresLoadError:
        'failed local extension update files load error',
  },
};

Object.assign(window, {extension_toolbar_tests});

suite(extension_toolbar_tests.suiteName, function() {
  let mockDelegate: TestService;
  let toolbar: ExtensionsToolbarElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    toolbar = document.createElement('extensions-toolbar');
    document.body.appendChild(toolbar);
    toolbar.inDevMode = false;
    toolbar.devModeControlledByPolicy = false;
    toolbar.isChildAccount = false;

    // <if expr="chromeos_ash">
    toolbar.kioskEnabled = false;
    // </if>

    mockDelegate = new TestService();
    toolbar.set('delegate', mockDelegate);

    // The toast manager is normally a child of the <extensions-manager>
    // element, so add it separately for this test.
    const toastManager = document.createElement('cr-toast-manager');
    document.body.appendChild(toastManager);
  });

  test(extension_toolbar_tests.TestNames.Layout, function() {
    const boundTestVisible = testVisible.bind(null, toolbar);
    boundTestVisible('#devMode', true);
    assertEquals(toolbar.$.devMode.disabled, false);
    boundTestVisible('#loadUnpacked', false);
    boundTestVisible('#packExtensions', false);
    boundTestVisible('#updateNow', false);
    toolbar.set('inDevMode', true);
    flush();

    boundTestVisible('#devMode', true);
    assertEquals(toolbar.$.devMode.disabled, false);
    boundTestVisible('#loadUnpacked', true);
    boundTestVisible('#packExtensions', true);
    boundTestVisible('#updateNow', true);

    toolbar.set('canLoadUnpacked', false);
    flush();

    boundTestVisible('#devMode', true);
    boundTestVisible('#loadUnpacked', false);
    boundTestVisible('#packExtensions', true);
    boundTestVisible('#updateNow', true);
  });

  test(extension_toolbar_tests.TestNames.DevModeToggle, function() {
    const toggle = toolbar.$.devMode;
    assertFalse(toggle.disabled);

    // Test that the dev-mode toggle is disabled when a policy exists.
    toolbar.set('devModeControlledByPolicy', true);
    flush();
    assertTrue(toggle.disabled);

    toolbar.set('devModeControlledByPolicy', false);
    flush();
    assertFalse(toggle.disabled);

    // Test that the dev-mode toggle is disabled for child account users.
    toolbar.set('isChildAccount', true);
    flush();
    assertTrue(toggle.disabled);
  });

  test(extension_toolbar_tests.TestNames.ClickHandlers, async function() {
    toolbar.set('inDevMode', true);
    flush();
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
    flush();
    const dialog = toolbar.shadowRoot!.querySelector('extensions-pack-dialog');
    assertTrue(!!dialog);
  });

  /** Tests that the update button properly fires the load-error event. */
  test(
      extension_toolbar_tests.TestNames.FailedUpdateFiresLoadError,
      async function() {
        const item = document.createElement('extensions-item');
        item.data = createExtensionInfo();
        item.delegate = mockDelegate;
        document.body.appendChild(item);
        item.set('inDevMode', true);
        item.set('data.location', chrome.developerPrivate.Location.UNPACKED);

        toolbar.set('inDevMode', true);
        flush();

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
        toolbar.$.updateNow.click();
        await proxyDelegate.whenCalled('updateAllExtensions');
        await verifyLoadErrorFired(false);

        proxyDelegate.resetResolver('updateAllExtensions');
        proxyDelegate.setForceReloadItemError(true);
        toolbar.$.updateNow.click();
        await proxyDelegate.whenCalled('updateAllExtensions');
        await verifyLoadErrorFired(true);
      });

  // <if expr="chromeos_ash">
  test(extension_toolbar_tests.TestNames.KioskMode, function() {
    const button = toolbar.$.kioskExtensions;
    assertTrue(button.hidden);
    toolbar.kioskEnabled = true;
    assertFalse(button.hidden);

    const whenTapped = eventToPromise('kiosk-tap', toolbar);
    button.click();
    return whenTapped;
  });
  // </if>
});

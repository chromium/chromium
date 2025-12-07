// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://signout-confirmation/signout_confirmation.js';

import {SignoutConfirmationBrowserProxyImpl} from 'chrome://signout-confirmation/signout_confirmation.js';
import type {ExtensionsSectionElement, PageRemote, SignoutConfirmationAppElement} from 'chrome://signout-confirmation/signout_confirmation.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import type {ModifiersParam} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

import {TestSignoutConfirmationBrowserProxy} from './test_signout_confirmation_browser_proxy.js';

suite('SignoutConfirmationViewTest', function() {
  let signoutConfirmationApp: SignoutConfirmationAppElement;
  let testProxy: TestSignoutConfirmationBrowserProxy;
  let callbackRouterRemote: PageRemote;

  function keyDown(item: HTMLElement, key: string, modifiers?: ModifiersParam) {
    keyDownOn(item, 0, modifiers, key);
  }

  setup(function() {
    testProxy = new TestSignoutConfirmationBrowserProxy();
    callbackRouterRemote =
        testProxy.callbackRouter.$.bindNewPipeAndPassRemote();
    SignoutConfirmationBrowserProxyImpl.setInstance(testProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    signoutConfirmationApp = document.createElement('signout-confirmation-app');
    document.body.append(signoutConfirmationApp);

    callbackRouterRemote.sendSignoutConfirmationData({
      dialogTitle: 'title',
      dialogSubtitle: 'subtitle',
      acceptButtonLabel: 'accept',
      cancelButtonLabel: 'cancel',
      accountExtensions: [],
      hasUnsyncedData: false,
    });

    return testProxy.handler.whenCalled('updateViewHeight');
  });

  test('HeaderContent', function() {
    assertTrue(isVisible(signoutConfirmationApp));

    // Header.
    assertTrue(isChildVisible(signoutConfirmationApp, '#header'));
    assertTrue(isChildVisible(signoutConfirmationApp, '#title'));
    assertTrue(isChildVisible(signoutConfirmationApp, '#subtitle'));

    // Buttons.
    assertTrue(isChildVisible(signoutConfirmationApp, '#acceptButton'));
    assertTrue(isChildVisible(signoutConfirmationApp, '#cancelButton'));
  });

  test('ExtensionsSectionVisible', async function() {
    assertTrue(isVisible(signoutConfirmationApp));

    // Extensions section should not be visible if there are no account
    // extensions.
    assertFalse(isChildVisible(signoutConfirmationApp, 'extensions-section'));

    // Reset the handler.
    testProxy.handler.reset();

    // Send an update containing one account extension.
    callbackRouterRemote.sendSignoutConfirmationData({
      dialogTitle: 'title',
      dialogSubtitle: 'subtitle',
      acceptButtonLabel: 'accept',
      cancelButtonLabel: 'cancel',
      accountExtensions: [{
        name: 'name',
        iconUrl: 'icon.png',
      }],
      hasUnsyncedData: false,
    });

    // Wait for the new data to actually be updated in the component by waiting
    // for a height update triggered by receipt of the new data.
    await testProxy.handler.whenCalled('updateViewHeight');

    // The extensions section should now be visible.
    const extensionsSection =
        signoutConfirmationApp.shadowRoot
            .querySelector<ExtensionsSectionElement>('extensions-section');

    assertTrue(!!extensionsSection);
    assertTrue(isVisible(extensionsSection));

    // Now check the checkbox in the extensions section.
    assertFalse(extensionsSection.checked());
    extensionsSection.$.checkbox.click();
    assertTrue(extensionsSection.checked());

    // Accept the dialog.
    signoutConfirmationApp.$.acceptButton.click();
    const uninstallAccountExtensions =
        await testProxy.handler.whenCalled('accept');
    assertTrue(uninstallAccountExtensions);
  });

  test('ClickAccept', async function() {
    assertTrue(isVisible(signoutConfirmationApp));
    signoutConfirmationApp.$.acceptButton.click();
    const uninstallAccountExtensions =
        await testProxy.handler.whenCalled('accept');
    assertFalse(uninstallAccountExtensions);
  });

  test('ClickCancel', async function() {
    assertTrue(isVisible(signoutConfirmationApp));
    signoutConfirmationApp.$.cancelButton.click();
    const uninstallAccountExtensions =
        await testProxy.handler.whenCalled('cancel');
    assertFalse(uninstallAccountExtensions);
  });

  test('CloseDialog', function() {
    assertTrue(isVisible(signoutConfirmationApp));
    keyDown(signoutConfirmationApp, 'Escape');
    return testProxy.handler.whenCalled('close');
  });

  test('UnsyncedAccountExtensionsText', async function() {
    assertTrue(isVisible(signoutConfirmationApp));

    // Additional text for unsynced account extensions should not be shown by
    // default (no unsynced data and no account extensions).
    assertFalse(
        isChildVisible(signoutConfirmationApp, '#unsyncedAccountExtensions'));

    // Reset the handler.
    testProxy.handler.reset();

    // Send an update with unsynced data but no account extensions.
    callbackRouterRemote.sendSignoutConfirmationData({
      dialogTitle: 'title',
      dialogSubtitle: 'subtitle',
      acceptButtonLabel: 'accept',
      cancelButtonLabel: 'cancel',
      accountExtensions: [],
      hasUnsyncedData: true,
    });

    // Wait for the new data to actually be updated in the component by waiting
    // for a height update triggered by receipt of the new data.
    await testProxy.handler.whenCalled('updateViewHeight');

    // Text should still not be shown because there are no account extensions.
    assertFalse(
        isChildVisible(signoutConfirmationApp, '#unsyncedAccountExtensions'));

    // Repeat the above except we now have an unsynced account extension.
    testProxy.handler.reset();
    callbackRouterRemote.sendSignoutConfirmationData({
      dialogTitle: 'title',
      dialogSubtitle: 'subtitle',
      acceptButtonLabel: 'accept',
      cancelButtonLabel: 'cancel',
      accountExtensions: [{
        name: 'name',
        iconUrl: 'icon.png',
      }],
      hasUnsyncedData: true,
    });
    await testProxy.handler.whenCalled('updateViewHeight');

    // Text should now be shown..
    assertTrue(
        isChildVisible(signoutConfirmationApp, '#extensionsAdditionalText'));
  });
});

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://signout-confirmation/signout_confirmation.js';

import {SignoutConfirmationBrowserProxyImpl} from 'chrome://signout-confirmation/signout_confirmation.js';
import type {PageRemote, SignoutConfirmationAppElement} from 'chrome://signout-confirmation/signout_confirmation.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import type {ModifiersParam} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {isChildVisible, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

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
    });

    return testProxy.handler.whenCalled('updateViewHeight');
  });

  test('HeaderContent', async function() {
    assertTrue(isVisible(signoutConfirmationApp));
    await microtasksFinished();

    // Header.
    assertTrue(isChildVisible(signoutConfirmationApp, '#header'));
    assertTrue(isChildVisible(signoutConfirmationApp, '#title'));
    assertTrue(isChildVisible(signoutConfirmationApp, '#subtitle'));

    // Buttons.
    assertTrue(isChildVisible(signoutConfirmationApp, '#acceptButton'));
    assertTrue(isChildVisible(signoutConfirmationApp, '#cancelButton'));
  });

  test('ClickAccept', function() {
    assertTrue(isVisible(signoutConfirmationApp));
    signoutConfirmationApp.$.acceptButton.click();
    return testProxy.handler.whenCalled('accept');
  });

  test('ClickCancel', function() {
    assertTrue(isVisible(signoutConfirmationApp));
    signoutConfirmationApp.$.cancelButton.click();
    return testProxy.handler.whenCalled('cancel');
  });

  test('CloseDialog', function() {
    assertTrue(isVisible(signoutConfirmationApp));
    keyDown(signoutConfirmationApp, 'Escape');
    return testProxy.handler.whenCalled('close');
  });
});

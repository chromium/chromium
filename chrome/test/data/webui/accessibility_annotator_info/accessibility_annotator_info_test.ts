// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://accessibility-annotator-info/accessibility_annotator_info.js';

import type {AccessibilityAnnotatorInfoElement} from 'chrome://accessibility-annotator-info/accessibility_annotator_info.js';
import {AccessibilityAnnotatorInfoBrowserProxy} from 'chrome://accessibility-annotator-info/browser_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestAccessibilityAnnotatorInfoBrowserProxy} from './test_accessibility_annotator_info_browser_proxy.js';

suite('AccessibilityAnnotatorInfoTest', function() {
  let accessibilityAnnotatorInfoElement: AccessibilityAnnotatorInfoElement;
  let browserProxy: TestAccessibilityAnnotatorInfoBrowserProxy;

  setup(async function() {
    browserProxy = new TestAccessibilityAnnotatorInfoBrowserProxy();
    AccessibilityAnnotatorInfoBrowserProxy.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    accessibilityAnnotatorInfoElement =
        document.createElement('accessibility-annotator-info');
    document.body.appendChild(accessibilityAnnotatorInfoElement);
    await microtasksFinished();
  });

  test('ManageSettingsClick', async function() {
    const manageSettingsButton =
        accessibilityAnnotatorInfoElement.shadowRoot
            ?.querySelector<HTMLElement>('#manageSettings');
    assertTrue(!!manageSettingsButton);
    manageSettingsButton.click();
    await browserProxy.handler.whenCalled('onManageSettingsClicked');
    assertEquals(
        1, browserProxy.handler.getCallCount('onManageSettingsClicked'));
  });

  test('GotItClick', async function() {
    const gotItButton = accessibilityAnnotatorInfoElement.shadowRoot
                            ?.querySelector<HTMLElement>('#gotIt');
    assertTrue(!!gotItButton);
    gotItButton.click();
    await browserProxy.handler.whenCalled('onInfoAcknowledged');
    assertEquals(1, browserProxy.handler.getCallCount('onInfoAcknowledged'));
  });

  test('RendersAccountInfo', async function() {
    const testEmail = 'test@example.com';
    const testAvatarUrl = 'https://example.com/avatar.png';
    browserProxy.handler.setAccountInfo({
      email: testEmail,
      avatarUrl: testAvatarUrl,
    });

    accessibilityAnnotatorInfoElement =
        document.createElement('accessibility-annotator-info');
    document.body.appendChild(accessibilityAnnotatorInfoElement);

    await browserProxy.handler.whenCalled('getAccountInfo');
    await microtasksFinished();

    const emailElement = accessibilityAnnotatorInfoElement.shadowRoot
                             ?.querySelector<HTMLElement>('#email');
    assertTrue(!!emailElement);
    assertEquals(testEmail, emailElement.textContent);

    const avatarElement = accessibilityAnnotatorInfoElement.shadowRoot
                              ?.querySelector<HTMLImageElement>('#avatar');
    assertTrue(!!avatarElement);
    assertEquals(testAvatarUrl, avatarElement.src);

    const accountInfoDiv = accessibilityAnnotatorInfoElement.shadowRoot
                               ?.querySelector<HTMLElement>('.account-info');
    assertTrue(!!accountInfoDiv);
    assertFalse(accountInfoDiv.hidden);
  });

  test('HidesAccountInfoWhenEmpty', async function() {
    accessibilityAnnotatorInfoElement =
        document.createElement('accessibility-annotator-info');
    document.body.appendChild(accessibilityAnnotatorInfoElement);

    await browserProxy.handler.whenCalled('getAccountInfo');
    await microtasksFinished();

    const accountInfoDiv = accessibilityAnnotatorInfoElement.shadowRoot
                               ?.querySelector<HTMLElement>('.account-info');
    assertTrue(!!accountInfoDiv);
    assertTrue(accountInfoDiv.hidden);
  });
});

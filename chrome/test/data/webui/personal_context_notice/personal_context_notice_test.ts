// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://accessibility-annotator-info/personal_context_notice.js';

import {PersonalContextNoticeBrowserProxy} from 'chrome://accessibility-annotator-info/browser_proxy.js';
import type {PersonalContextNoticeElement} from 'chrome://accessibility-annotator-info/personal_context_notice.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPersonalContextNoticeBrowserProxy} from './test_personal_context_notice_browser_proxy.js';

suite('AccessibilityAnnotatorInfoTest', function() {
  let personalContextNoticeElement: PersonalContextNoticeElement;
  let browserProxy: TestPersonalContextNoticeBrowserProxy;

  setup(async function() {
    browserProxy = new TestPersonalContextNoticeBrowserProxy();
    PersonalContextNoticeBrowserProxy.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    personalContextNoticeElement =
        document.createElement('personal-context-notice');
    document.body.appendChild(personalContextNoticeElement);
    await microtasksFinished();
  });

  test('ManageSettingsClick', async function() {
    const manageSettingsButton =
        personalContextNoticeElement.shadowRoot?.querySelector<HTMLElement>(
            '#manageSettings');
    assertTrue(!!manageSettingsButton);
    manageSettingsButton.click();
    await browserProxy.handler.whenCalled('onManageSettingsClicked');
    assertEquals(
        1, browserProxy.handler.getCallCount('onManageSettingsClicked'));
  });

  test('GotItClick', async function() {
    const gotItButton =
        personalContextNoticeElement.shadowRoot?.querySelector<HTMLElement>(
            '#gotIt');
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

    personalContextNoticeElement =
        document.createElement('personal-context-notice');
    document.body.appendChild(personalContextNoticeElement);

    await browserProxy.handler.whenCalled('getAccountInfo');
    await microtasksFinished();

    const emailElement =
        personalContextNoticeElement.shadowRoot?.querySelector<HTMLElement>(
            '#email');
    assertTrue(!!emailElement);
    assertEquals(testEmail, emailElement.textContent);

    const avatarElement = personalContextNoticeElement.shadowRoot
                              ?.querySelector<HTMLImageElement>('#avatar');
    assertTrue(!!avatarElement);
    assertEquals(testAvatarUrl, avatarElement.src);

    const accountInfoDiv =
        personalContextNoticeElement.shadowRoot?.querySelector<HTMLElement>(
            '.account-info');
    assertTrue(!!accountInfoDiv);
    assertFalse(accountInfoDiv.hidden);
  });

  test('HidesAccountInfoWhenEmpty', async function() {
    personalContextNoticeElement =
        document.createElement('personal-context-notice');
    document.body.appendChild(personalContextNoticeElement);

    await browserProxy.handler.whenCalled('getAccountInfo');
    await microtasksFinished();

    const accountInfoDiv =
        personalContextNoticeElement.shadowRoot?.querySelector<HTMLElement>(
            '.account-info');
    assertTrue(!!accountInfoDiv);
    assertTrue(accountInfoDiv.hidden);
  });

  test('RendersTriggerCard', function() {
    const triggerCard =
        personalContextNoticeElement.shadowRoot?.querySelector<HTMLElement>(
            '#triggerCard');
    assertTrue(!!triggerCard);

    // Assert the formatting is there.
    const pillFormatting = triggerCard.querySelector<HTMLElement>('.pill');
    assertTrue(!!pillFormatting);

    // Assert the constructed string is correct.
    const textWithPlaceholder =
        loadTimeData.getString('accessibilityAnnotatorInfoCard1');
    const placeholder =
        loadTimeData.getString('accessibilityAnnotatorTriggerText');
    const expectedText = textWithPlaceholder.replace('$1', placeholder);
    const actualText =
        (triggerCard.textContent || '').replace(/\s+/g, ' ').trim();
    assertEquals(expectedText, actualText);
  });
});

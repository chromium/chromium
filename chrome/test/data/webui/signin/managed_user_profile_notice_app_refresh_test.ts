// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://managed-user-profile-notice/managed_user_profile_notice_app_refresh.js';

import type {ManagedUserProfileNoticeAppRefreshElement} from 'chrome://managed-user-profile-notice/managed_user_profile_notice_app_refresh.js';
import type {ManagedUserProfileInfo} from 'chrome://managed-user-profile-notice/managed_user_profile_notice_browser_proxy.js';
import {ManagedUserProfileNoticeBrowserProxyImpl, State} from 'chrome://managed-user-profile-notice/managed_user_profile_notice_browser_proxy.js';
import type {ManagedUserProfileNoticeDisclosureRefreshElement} from 'chrome://managed-user-profile-notice/managed_user_profile_notice_disclosure_refresh.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestManagedUserProfileNoticeBrowserProxy} from './test_managed_user_profile_notice_browser_proxy.js';

suite('ManagedUserProfileNoticeRefreshTest', function() {
  let app: ManagedUserProfileNoticeAppRefreshElement;
  let browserProxy: TestManagedUserProfileNoticeBrowserProxy;

  const AVATAR_URL_1: string = 'chrome://theme/IDR_PROFILE_AVATAR_1';
  const AVATAR_URL_2: string = 'chrome://theme/IDR_PROFILE_AVATAR_2';

  const testManagedUserProfileInfo: ManagedUserProfileInfo = {
    pictureUrl: AVATAR_URL_1,
    showEnterpriseBadge: false,
    title: 'title',
    subtitle: 'subtitle',
    proceedLabel: 'proceed_label',
    accountName: 'account_name',
    continueAs: 'continue_as',
    email: 'email@email.com',
    checkLinkDataCheckboxByDefault: false,
  };

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestManagedUserProfileNoticeBrowserProxy(
        testManagedUserProfileInfo);
    ManagedUserProfileNoticeBrowserProxyImpl.setInstance(browserProxy);
    loadTimeData.overrideValues({'showLinkDataCheckbox': false});
    app = document.createElement('managed-user-profile-notice-app-refresh');
    document.body.appendChild(app);
    await browserProxy.whenCalled('initialized');
    return microtasksFinished();
  });

  /**
   * Checks that the expected image url is displayed.
   */
  function checkImageUrl(
      targetElement: ManagedUserProfileNoticeDisclosureRefreshElement,
      expectedUrl: string) {
    assertTrue(isVisible(targetElement.$.avatar));
    assertEquals(expectedUrl, targetElement.$.avatar.src);
  }

  test('proceed', async function() {
    assertTrue(isVisible(app.$.proceedButton));
    app.$.proceedButton.click();
    await browserProxy.whenCalled('proceed');
  });

  test('cancel', async function() {
    assertTrue(isVisible(app.$.cancelButton));
    app.$.cancelButton.click();
    await browserProxy.whenCalled('cancel');
  });

  test('stateChanges', async function() {
    async function checkState(
        state: State, visibleElements: string[], hiddenElements: string[],
        expectedProceedLabel?: string) {
      webUIListenerCallback('on-state-changed', state);
      await microtasksFinished();
      const stateName = State[state];
      for (const id of visibleElements) {
        assertTrue(
            isChildVisible(app, id),
            `${stateName} State: ${id} should be visible`);
      }
      for (const id of hiddenElements) {
        assertFalse(
            isChildVisible(app, id),
            `${stateName} State: ${id} should be hidden`);
      }
      if (expectedProceedLabel) {
        assertEquals(
            expectedProceedLabel, app.$.proceedButton.textContent.trim(),
            `${stateName} State: Proceed label`);
      }
    }

    const allSections = [
      '#disclosure',
      '#processing',
      '#error',
      '#timeout',
      '#success',
    ];

    await checkState(
        State.DISCLOSURE, ['#disclosure', '#proceedButton', '#cancelButton'],
        allSections.filter(id => id !== '#disclosure'),
        testManagedUserProfileInfo.proceedLabel);

    await checkState(
        State.PROCESSING, ['#processing', '#cancelButton'],
        [...allSections.filter(id => id !== '#processing'), '#proceedButton']);

    await checkState(
        State.ERROR, ['#error', '#proceedButton'],
        [...allSections.filter(id => id !== '#error'), '#cancelButton'],
        app.i18n('closeLabel'));

    await checkState(
        State.TIMEOUT, ['#timeout', '#proceedButton', '#cancelButton'],
        allSections.filter(id => id !== '#timeout'), app.i18n('retryLabel'));

    await checkState(
        State.SUCCESS, ['#success', '#proceedButton'],
        [...allSections.filter(id => id !== '#success'), '#cancelButton'],
        testManagedUserProfileInfo.proceedLabel);
  });

  test('onProfileInfoChangedDisclosureSection', async function() {
    // Navigate to the disclosure section.
    webUIListenerCallback('on-state-changed', State.DISCLOSURE);
    await microtasksFinished();

    const targetElement =
        app.shadowRoot
            .querySelector<ManagedUserProfileNoticeDisclosureRefreshElement>(
                '#disclosure');
    assertTrue(!!targetElement);

    // Initial values.
    assertTrue(isVisible(targetElement.$.title));
    assertEquals(
        app.i18n('profileDisclosureTitle'),
        targetElement.$.title.textContent.trim());
    assertTrue(isVisible(targetElement.$.subtitle));
    assertEquals(
        app.i18n('profileDisclosureSubtitle'),
        targetElement.$.subtitle.textContent.trim());
    assertTrue(isVisible(app.$.proceedButton));
    assertEquals(
        testManagedUserProfileInfo.proceedLabel,
        app.$.proceedButton.textContent.trim());

    checkImageUrl(targetElement, AVATAR_URL_1);
    assertFalse(isChildVisible(targetElement, '.work-badge'));

    // Update the values.
    const newProfileInfo: ManagedUserProfileInfo = {
      pictureUrl: AVATAR_URL_2,
      showEnterpriseBadge: true,
      title: 'new_title',
      subtitle: 'new_subtitle',
      proceedLabel: 'new_proceed_label',
      accountName: 'new_account_name',
      continueAs: 'new_continue_as',
      email: 'new_email@email.com',
      checkLinkDataCheckboxByDefault: false,
    };
    webUIListenerCallback('on-profile-info-changed', newProfileInfo);
    await microtasksFinished();

    assertTrue(isVisible(targetElement.$.title));
    assertEquals(
        app.i18n('profileDisclosureTitle'),
        targetElement.$.title.textContent.trim());
    assertTrue(isVisible(targetElement.$.subtitle));
    assertEquals(
        app.i18n('profileDisclosureSubtitle'),
        targetElement.$.subtitle.textContent.trim());
    assertTrue(isVisible(app.$.proceedButton));
    assertEquals(
        newProfileInfo.proceedLabel, app.$.proceedButton.textContent.trim());

    checkImageUrl(targetElement, AVATAR_URL_2);
    assertTrue(isChildVisible(targetElement, '.work-badge'));
    assertTrue(isVisible(app.$.cancelButton));
  });
});

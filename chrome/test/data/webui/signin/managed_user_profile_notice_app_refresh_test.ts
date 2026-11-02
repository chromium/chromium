// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://managed-user-profile-notice/managed_user_profile_notice_app_refresh.js';

import type {ManagedUserProfileNoticeAppRefreshElement} from 'chrome://managed-user-profile-notice/managed_user_profile_notice_app_refresh.js';
import type {ManagedUserProfileInfo} from 'chrome://managed-user-profile-notice/managed_user_profile_notice_browser_proxy.js';
import {ManagedUserProfileNoticeBrowserProxyImpl, ScreenType, State} from 'chrome://managed-user-profile-notice/managed_user_profile_notice_browser_proxy.js';
import type {ManagedUserProfileNoticeDisclosureRefreshElement} from 'chrome://managed-user-profile-notice/managed_user_profile_notice_disclosure_refresh.js';
import type {ManagedUserProfileNoticeStateRefreshElement} from 'chrome://managed-user-profile-notice/managed_user_profile_notice_state_refresh.js';
import type {SignalsDisclaimerRefreshElement} from 'chrome://managed-user-profile-notice/signals_disclaimer_refresh.js';
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
    await createManagedUserProfileNoticeApp();
    return microtasksFinished();
  });

  function createManagedUserProfileNoticeApp() {
    app = document.createElement('managed-user-profile-notice-app-refresh');
    document.body.appendChild(app);
    return browserProxy.whenCalled('initialized');
  }

  /**
   * Checks that the expected image url is displayed.
   */
  function checkImageUrl(
      targetElement: ManagedUserProfileNoticeDisclosureRefreshElement|
      SignalsDisclaimerRefreshElement,
      expectedUrl: string) {
    assertTrue(isVisible(targetElement.$.avatar));
    assertEquals(expectedUrl, targetElement.$.avatar.src);
  }

  /**
   * Checks that the state section is visible and shows the expected icon,
   * title and subtitle. An empty `expectedTitle` means that no title is shown.
   */
  function checkStateSection(
      iconSelector: string, expectedTitle: string,
      expectedSubtitle: string): void {
    const stateElement =
        app.shadowRoot
            .querySelector<ManagedUserProfileNoticeStateRefreshElement>(
                '#state');
    assertTrue(!!stateElement);
    assertTrue(isVisible(stateElement));
    assertTrue(isChildVisible(stateElement, iconSelector));

    if (expectedTitle) {
      assertTrue(isVisible(stateElement.$.title));
      assertEquals(expectedTitle, stateElement.$.title.textContent.trim());
    } else {
      assertFalse(isVisible(stateElement.$.title));
    }
    assertTrue(isVisible(stateElement.$.subtitle));
    assertEquals(expectedSubtitle, stateElement.$.subtitle.textContent.trim());
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
      '#signals-disclaimer',
      '#state',
    ];

    await checkState(
        State.DISCLOSURE, ['#disclosure', '#proceedButton', '#cancelButton'],
        allSections.filter(id => id !== '#disclosure'),
        app.i18n('continueLabel'));

    await checkState(
        State.SIGNALS_DISCLAIMER,
        ['#signals-disclaimer', '#proceedButton', '#cancelButton'],
        allSections.filter(id => id !== '#signals-disclaimer'),
        app.i18n('signalsDisclaimerContinueLabel'));

    await checkState(
        State.PROCESSING, ['#state', '#cancelButton'],
        [...allSections.filter(id => id !== '#state'), '#proceedButton']);

    await checkState(
        State.ERROR, ['#state', '#proceedButton'],
        [...allSections.filter(id => id !== '#state'), '#cancelButton'],
        app.i18n('closeLabel'));

    await checkState(
        State.TIMEOUT, ['#state', '#proceedButton', '#cancelButton'],
        allSections.filter(id => id !== '#state'), app.i18n('retryLabel'));

    await checkState(
        State.SUCCESS, ['#state', '#proceedButton'],
        [...allSections.filter(id => id !== '#state'), '#cancelButton'],
        app.i18n('continueLabel'));
  });

  test('stateSection', async function() {
    webUIListenerCallback('on-state-changed', State.PROCESSING);
    await microtasksFinished();
    checkStateSection(
        '.spinner', '', loadTimeData.getString('processingSubtitle'));

    webUIListenerCallback('on-long-processing');
    await microtasksFinished();
    checkStateSection('.spinner', '', app.i18n('longProcessingSubtitle'));

    webUIListenerCallback('on-state-changed', State.TIMEOUT);
    await microtasksFinished();
    checkStateSection(
        '.timeout-icon', app.i18n('timeoutTitle'), app.i18n('timeoutSubtitle'));

    webUIListenerCallback('on-state-changed', State.SUCCESS);
    await microtasksFinished();
    checkStateSection(
        '.success-icon', app.i18n('successTitle'), app.i18n('successSubtitle'));
  });

  test('stateSectionError', async function() {
    // Errors are reported by the OIDC enrollment flow, after the processing
    // state is shown.
    webUIListenerCallback('on-state-changed', State.PROCESSING);
    await microtasksFinished();

    webUIListenerCallback(
        'on-state-changed-to-error', 'error_title', 'error_subtitle');
    await microtasksFinished();

    checkStateSection('.error-icon', 'error_title', 'error_subtitle');
    assertEquals(
        app.i18n('closeLabel'), app.$.proceedButton.textContent.trim());
  });

  test('signalsDisclaimerSection', async function() {
    webUIListenerCallback('on-state-changed', State.SIGNALS_DISCLAIMER);
    await microtasksFinished();

    const targetElement =
        app.shadowRoot.querySelector<SignalsDisclaimerRefreshElement>(
            '#signals-disclaimer');
    assertTrue(!!targetElement);

    assertTrue(isVisible(targetElement.$.title));
    assertEquals(
        app.i18n('signalsDisclaimerTitle'),
        targetElement.$.title.textContent.trim());
    assertTrue(isVisible(targetElement.$.subtitle));
    assertTrue(targetElement.$.subtitle.textContent.includes(
        app.i18n('signalsDisclaimerSubtitle')));
    assertTrue(isVisible(app.$.proceedButton));
    assertEquals(
        app.i18n('signalsDisclaimerContinueLabel'),
        app.$.proceedButton.textContent.trim());
    assertTrue(isVisible(app.$.cancelButton));
    assertEquals(
        app.i18n('signalsDisclaimerCancelLabel'),
        app.$.cancelButton.textContent.trim());

    checkImageUrl(targetElement, AVATAR_URL_1);
    assertTrue(isChildVisible(targetElement, '.work-badge'));

    assertTrue(isVisible(targetElement.$.learnMoreLink));
    targetElement.$.learnMoreLink.click();
    await browserProxy.whenCalled('learnMoreClicked');
  });

  test('signalsDisclaimerKeyboardInteractions', async function() {
    webUIListenerCallback('on-state-changed', State.SIGNALS_DISCLAIMER);
    await microtasksFinished();

    const targetElement =
        app.shadowRoot.querySelector<SignalsDisclaimerRefreshElement>(
            '#signals-disclaimer');
    assertTrue(!!targetElement);
    const learnMoreLink = targetElement.$.learnMoreLink;
    assertTrue(isVisible(learnMoreLink));

    assertEquals(0, learnMoreLink.tabIndex);

    function pressKey(key: string): KeyboardEvent {
      const event = new KeyboardEvent(
          'keydown', {key, bubbles: true, cancelable: true, composed: true});
      learnMoreLink.dispatchEvent(event);
      return event;
    }

    // Only Enter and Space keys should activate the link.
    assertFalse(pressKey('q').defaultPrevented);
    assertEquals(0, browserProxy.getCallCount('learnMoreClicked'));

    assertTrue(pressKey('Enter').defaultPrevented);
    assertEquals(1, browserProxy.getCallCount('learnMoreClicked'));

    assertTrue(pressKey(' ').defaultPrevented);
    assertEquals(2, browserProxy.getCallCount('learnMoreClicked'));
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
        app.i18n('continueLabel'), app.$.proceedButton.textContent.trim());

    checkImageUrl(targetElement, AVATAR_URL_1);
    assertFalse(isChildVisible(targetElement, '.work-badge'));

    // Update the values.
    const newProfileInfo: ManagedUserProfileInfo = {
      pictureUrl: AVATAR_URL_2,
      showEnterpriseBadge: true,
      title: 'new_title',
      subtitle: 'new_subtitle',
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
        app.i18n('continueLabel'), app.$.proceedButton.textContent.trim());

    checkImageUrl(targetElement, AVATAR_URL_2);
    assertTrue(isChildVisible(targetElement, '.work-badge'));
    assertTrue(isVisible(app.$.cancelButton));
  });

  test('change animation file depending on the theme', async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // The animation is only shown during the first run with the revamp enabled.
    loadTimeData.overrideValues({
      'screenType': ScreenType.FIRST_RUN,
    });
    await createManagedUserProfileNoticeApp();

    // The animation is only shown in the disclosure section.
    webUIListenerCallback('on-state-changed', State.DISCLOSURE);
    await microtasksFinished();

    const targetElement =
        app.shadowRoot
            .querySelector<ManagedUserProfileNoticeDisclosureRefreshElement>(
                '#disclosure');
    assertTrue(!!targetElement);

    browserProxy.setMatchMediaMatches(false);
    await microtasksFinished();

    const avatarAnimation =
        targetElement.shadowRoot.querySelector<HTMLElement>('#avatarAnimation');
    assertTrue(!!avatarAnimation);
    assertTrue(isVisible(avatarAnimation));
    const animationUrlLight = avatarAnimation.getAttribute('animation-url');
    assertTrue(!!animationUrlLight);
    assertFalse(animationUrlLight.includes('dark'));

    browserProxy.setMatchMediaMatches(true);
    await microtasksFinished();

    const animationUrlDark = avatarAnimation.getAttribute('animation-url');
    assertTrue(!!animationUrlDark);
    assertTrue(animationUrlDark.includes('dark'));
  });
});

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://managed-user-profile-notice/legacy_managed_user_profile_notice_app.js';
import 'chrome://managed-user-profile-notice/managed_user_profile_notice_app.js';
import 'chrome://managed-user-profile-notice/managed_user_profile_notice_disclosure.js';

import type {LegacyManagedUserProfileNoticeAppElement} from 'chrome://managed-user-profile-notice/legacy_managed_user_profile_notice_app.js';
import type {ManagedUserProfileNoticeAppElement} from 'chrome://managed-user-profile-notice/managed_user_profile_notice_app.js';
import type {ManagedUserProfileInfo} from 'chrome://managed-user-profile-notice/managed_user_profile_notice_browser_proxy.js';
import {ManagedUserProfileNoticeBrowserProxyImpl, State} from 'chrome://managed-user-profile-notice/managed_user_profile_notice_browser_proxy.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestManagedUserProfileNoticeBrowserProxy} from './test_managed_user_profile_notice_browser_proxy.js';


[true, false].forEach(useUpdatedUi => {
  const suitePrefix = useUpdatedUi ? '' : 'Legacy';

  suite(`${suitePrefix}ManagedUserProfileNoticeTest`, function() {
    let app: LegacyManagedUserProfileNoticeAppElement|
        ManagedUserProfileNoticeAppElement;
    let browserProxy: TestManagedUserProfileNoticeBrowserProxy;

    const AVATAR_URL_1: string = 'chrome://theme/IDR_PROFILE_AVATAR_1';
    const AVATAR_URL_2: string = 'chrome://theme/IDR_PROFILE_AVATAR_2';

    const testManagedUserProfileInfo: ManagedUserProfileInfo = {
      pictureUrl: AVATAR_URL_1,
      showEnterpriseBadge: false,
      title: 'title',
      subtitle: 'subtitle',
      enterpriseInfo: 'enterprise_info',
      proceedLabel: 'proceed_label',
      accountName: 'account_name',
      continueAs: 'continue_as',
      email: 'email@email.com',
      checkLinkDataCheckboxByDefault: false,
    };

    setup(async function() {
      browserProxy = new TestManagedUserProfileNoticeBrowserProxy(
          testManagedUserProfileInfo);
      ManagedUserProfileNoticeBrowserProxyImpl.setInstance(browserProxy);
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      loadTimeData.overrideValues({'useUpdatedUi': useUpdatedUi});
      if (useUpdatedUi) {
        app = document.createElement('managed-user-profile-notice-app');
      } else {
        app = document.createElement('legacy-managed-user-profile-notice-app');
      }
      document.body.appendChild(app);
      return browserProxy.whenCalled('initialized');
    });

    teardown(function() {
      loadTimeData.overrideValues({'showLinkDataCheckbox': false});
    });

    /**
     * Checks that the expected image url is displayed.
     */
    function checkImageUrl(targetElement: HTMLElement, expectedUrl: string) {
      assertTrue(isChildVisible(targetElement, '#avatar'));
      const img =
          targetElement.shadowRoot!.querySelector<HTMLImageElement>('#avatar')!;
      assertEquals(expectedUrl, img.src);
    }

    test('proceed', async function() {
      assertTrue(isChildVisible(app, '#proceed-button'));
      app.shadowRoot!.querySelector<HTMLElement>('#proceed-button')!.click();
      await browserProxy.whenCalled('proceed');
    });

    test('cancel', async function() {
      assertTrue(isChildVisible(app, '#cancel-button'));
      app.shadowRoot!.querySelector<HTMLElement>('#cancel-button')!.click();
      await browserProxy.whenCalled('cancel');
    });

    test('linkData', async function() {
      if (useUpdatedUi) {
        return;
      }

      assertTrue(isChildVisible(app, '#proceed-button'));
      assertFalse(isChildVisible(app, '#linkData'));

      loadTimeData.overrideValues({'showLinkDataCheckbox': true});

      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      app = document.createElement('legacy-managed-user-profile-notice-app');
      document.body.appendChild(app);
      await browserProxy.whenCalled('initialized');

      assertTrue(isChildVisible(app, '#proceed-button'));
      assertTrue(isChildVisible(app, '#linkData'));

      const linkDataCheckbox: CrCheckboxElement =
          app.shadowRoot!.querySelector('#linkData')!;
      assertEquals(
          app.i18n('linkDataText'), linkDataCheckbox.textContent!.trim());
      assertFalse(linkDataCheckbox.checked);

      linkDataCheckbox.click();
      await microtasksFinished();
      const proceedButton =
          app.shadowRoot!.querySelector<HTMLElement>('#proceed-button')!;

      assertTrue(linkDataCheckbox.checked);
      assertEquals(
          app.i18n('continueLabel'), proceedButton.textContent!.trim());
    });

    test('linkDataCheckedByDefault', async function() {
      if (useUpdatedUi) {
        return;
      }

      assertTrue(isChildVisible(app, '#proceed-button'));
      assertFalse(isChildVisible(app, '#linkData'));
      loadTimeData.overrideValues({'showLinkDataCheckbox': true});

      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      app = document.createElement('legacy-managed-user-profile-notice-app');
      document.body.appendChild(app);
      await browserProxy.whenCalled('initialized');

      assertTrue(isChildVisible(app, '#proceed-button'));
      assertTrue(isChildVisible(app, '#linkData'));

      const linkDataCheckbox: CrCheckboxElement =
          app.shadowRoot!.querySelector('#linkData')!;
      assertEquals(
          app.i18n('linkDataText'), linkDataCheckbox.textContent!.trim());
      assertFalse(linkDataCheckbox.checked);

      // Update the values so that the lkink data checkbox is checked by
      // default.
      webUIListenerCallback('on-profile-info-changed', {
        pictureUrl: AVATAR_URL_1,
        showEnterpriseBadge: false,
        title: 'title',
        subtitle: 'subtitle',
        enterpriseInfo: 'enterprise_info',
        proceedLabel: 'proceed_label',
        accountName: 'account_name',
        continueAs: 'continue_as',
        email: 'email@email.com',
        checkLinkDataCheckboxByDefault: true,
      });
      await microtasksFinished();

      const proceedButton =
          app.shadowRoot!.querySelector<HTMLElement>('#proceed-button')!;

      assertTrue(linkDataCheckbox.checked);
      assertEquals(
          app.i18n('continueLabel'), proceedButton.textContent!.trim());

      // We should be able to uncheck it.
      linkDataCheckbox.click();
      await microtasksFinished();
      assertEquals(
          app.i18n('linkDataText'), linkDataCheckbox.textContent!.trim());
      assertFalse(linkDataCheckbox.checked);
    });

    test('stateChanges', async function() {
      if (!useUpdatedUi) {
        return;
      }

      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      app = document.createElement('managed-user-profile-notice-app');
      document.body.appendChild(app);
      await browserProxy.whenCalled('initialized');

      const proceedButton =
          app.shadowRoot!.querySelector<HTMLElement>('#proceed-button')!;

      webUIListenerCallback('on-state-changed', State.VALUE_PROPOSITION);
      await microtasksFinished();
      assertTrue(
          isChildVisible(app, '#value-prop'),
          'Value proposition State: #value-prop');
      assertFalse(
          isChildVisible(app, '#disclosure'),
          'Value proposition State: #disclosure');
      assertFalse(
          isChildVisible(app, '#user-data-handling'),
          'Disclosure State: #user-data-handling');
      assertFalse(
          isChildVisible(app, '#processing'),
          'Value proposition State: #processing');
      assertFalse(
          isChildVisible(app, '#error'), 'Value proposition State: #error');
      assertFalse(
          isChildVisible(app, '#timeout'), 'Value proposition State: #timeout');
      assertFalse(
          isChildVisible(app, '#success'), 'Value proposition State: #success');
      assertTrue(
          isChildVisible(app, '#proceed-button'),
          'Value proposition State: #proceed-button');
      assertTrue(
          isChildVisible(app, '#cancel-button'),
          'Value proposition State: #cancel-button');
      assertEquals(
          'continue_as', proceedButton.textContent!.trim(),
          'Value proposition State: Proceed label');

      webUIListenerCallback('on-state-changed', State.DISCLOSURE);
      await microtasksFinished();
      assertFalse(
          isChildVisible(app, '#value-prop'), 'Disclosure State: #value-prop');
      assertTrue(
          isChildVisible(app, '#disclosure'), 'Disclosure State: #disclosure');
      assertFalse(
          isChildVisible(app, '#user-data-handling'),
          'Disclosure State: #user-data-handling');
      assertFalse(
          isChildVisible(app, '#processing'), 'Disclosure State: #processing');
      assertFalse(isChildVisible(app, '#error'), 'Disclosure State: #error');
      assertFalse(
          isChildVisible(app, '#timeout'), 'Disclosure State: #timeout');
      assertFalse(
          isChildVisible(app, '#success'), 'Disclosure State: #success');
      assertTrue(
          isChildVisible(app, '#proceed-button'),
          'Disclosure State: #proceed-button');
      assertTrue(
          isChildVisible(app, '#cancel-button'),
          'Disclosure State: #cancel-button');
      assertEquals(
          app.i18n('continueLabel'), proceedButton.textContent!.trim(),
          'Disclosure State: Proceed label');

      webUIListenerCallback('on-state-changed', State.USER_DATA_HANDLING);
      await microtasksFinished();
      assertFalse(
          isChildVisible(app, '#value-prop'),
          'User Data Handling State: #value-prop');
      assertFalse(
          isChildVisible(app, '#disclosure'),
          'User Data Handling State: #disclosure');
      assertTrue(
          isChildVisible(app, '#user-data-handling'),
          'User Data Handling State: #user-data-handling');
      assertFalse(
          isChildVisible(app, '#processing'),
          'User Data Handling State: #processing');
      assertFalse(
          isChildVisible(app, '#error'), 'User Data Handling State: #error');
      assertFalse(
          isChildVisible(app, '#timeout'),
          'User Data Handling State: #timeout');
      assertFalse(
          isChildVisible(app, '#success'),
          'User Data Handling State: #success');
      assertTrue(
          isChildVisible(app, '#proceed-button'),
          'User Data Handling State: #proceed-button');
      assertTrue(
          isChildVisible(app, '#cancel-button'),
          'User Data Handling State: #cancel-button');
      assertEquals(
          app.i18n('confirmLabel'), proceedButton.textContent!.trim(),
          'User Data Handling State: Proceed label');

      webUIListenerCallback('on-state-changed', State.PROCESSING);
      await microtasksFinished();
      assertFalse(
          isChildVisible(app, '#value-prop'), 'Processing State: #value-prop');
      assertFalse(
          isChildVisible(app, '#disclosure'), 'Processing State: #disclosure');
      assertFalse(
          isChildVisible(app, '#user-data-handling'),
          'Disclosure State: #user-data-handling');
      assertTrue(
          isChildVisible(app, '#processing'), 'Processing State: #processing');
      assertFalse(isChildVisible(app, '#error'), 'Processing State: #error');
      assertFalse(
          isChildVisible(app, '#timeout'), 'Processing State: #timeout');
      assertFalse(
          isChildVisible(app, '#success'), 'Processing State: #success');
      assertFalse(
          isChildVisible(app, '#proceed-button'),
          'Processing State: #proceed-button');
      assertFalse(
          isChildVisible(app, '#cancel-button'),
          'Processing State: #cancel-button');

      webUIListenerCallback('on-state-changed', State.ERROR);
      await microtasksFinished();
      assertFalse(
          isChildVisible(app, '#value-prop'), 'Error State: #value-prop');
      assertFalse(
          isChildVisible(app, '#disclosure'), 'Error State: #disclosure');
      assertFalse(
          isChildVisible(app, '#user-data-handling'),
          'Disclosure State: #user-data-handling');
      assertFalse(
          isChildVisible(app, '#processing'), 'Error State: #processing');
      assertTrue(isChildVisible(app, '#error'), 'Error State: #error');
      assertFalse(isChildVisible(app, '#timeout'), 'Error State: #timeout');
      assertFalse(isChildVisible(app, '#success'), 'Error State: #success');
      assertTrue(
          isChildVisible(app, '#proceed-button'),
          'Error State: #proceed-button');
      assertFalse(
          isChildVisible(app, '#cancel-button'), 'Error State: #cancel-button');
      assertEquals(
          app.i18n('confirmLabel'), proceedButton.textContent!.trim(),
          'Error State: Proceed label');

      webUIListenerCallback('on-state-changed', State.TIMEOUT);
      await microtasksFinished();
      assertFalse(
          isChildVisible(app, '#value-prop'), 'Timeout State: #value-prop');
      assertFalse(
          isChildVisible(app, '#disclosure'), 'Timeout State: #disclosure');
      assertFalse(
          isChildVisible(app, '#user-data-handling'),
          'Disclosure State: #user-data-handling');
      assertFalse(
          isChildVisible(app, '#processing'), 'Timeout State: #processing');
      assertFalse(isChildVisible(app, '#error'), 'Timeout State: #error');
      assertTrue(isChildVisible(app, '#timeout'), 'Timeout State: #timeout');
      assertFalse(isChildVisible(app, '#success'), 'Timeout State: #success');
      assertTrue(
          isChildVisible(app, '#proceed-button'),
          'Timeout State: #proceed-button');
      assertFalse(
          isChildVisible(app, '#cancel-button'),
          'Timeout State: #cancel-button');
      assertEquals(
          app.i18n('confirmLabel'), proceedButton.textContent!.trim(),
          'Timeout State: Proceed label');

      webUIListenerCallback('on-state-changed', State.SUCCESS);
      await microtasksFinished();
      assertFalse(
          isChildVisible(app, '#value-prop'), 'Success State: #value-prop');
      assertFalse(
          isChildVisible(app, '#disclosure'), 'Success State: #disclosure');
      assertFalse(
          isChildVisible(app, '#user-data-handling'),
          'Disclosure State: #user-data-handling');
      assertFalse(
          isChildVisible(app, '#processing'), 'Success State: #processing');
      assertFalse(isChildVisible(app, '#error'), 'Success State: #error');
      assertFalse(isChildVisible(app, '#timeout'), 'Success State: #timeout');
      assertTrue(isChildVisible(app, '#success'), 'Success State: #success');
      assertTrue(
          isChildVisible(app, '#proceed-button'),
          'Success State: #proceed-button');
      assertFalse(
          isChildVisible(app, '#cancel-button'),
          'Success State: #cancel-button');
      assertEquals(
          app.i18n('confirmLabel'), proceedButton.textContent!.trim(),
          'Success State: Proceed label');
    });

    test('onProfileInfoChangedDisclosureSection', async function() {
      // Navigate to the disclosure section.
      webUIListenerCallback('on-state-changed', State.DISCLOSURE);
      await microtasksFinished();

      const targetElement = useUpdatedUi ?
          app.shadowRoot!.querySelector<HTMLElement>('#disclosure')! :
          app;
      // Helper to test all the text values in the UI.
      function checkTextValues(
          expectedTitle: string, expectedSubtitle: string,
          expectedEnterpriseInfo: string, expectedProceedLabel: string) {
        assertTrue(isChildVisible(targetElement, '.title'));
        const titleElement =
            targetElement.shadowRoot!.querySelector<HTMLElement>('.title')!;
        assertEquals(expectedTitle, titleElement.textContent!.trim());
        assertTrue(isChildVisible(targetElement, '.subtitle'));
        const subtitleElement =
            targetElement.shadowRoot!.querySelector<HTMLElement>('.subtitle')!;
        assertEquals(expectedSubtitle, subtitleElement.textContent!.trim());

        if (!useUpdatedUi) {
          assertTrue(isChildVisible(targetElement, '#enterpriseInfo'));
          const enterpriseInfoElement =
              targetElement.shadowRoot!.querySelector<HTMLElement>(
                  '#enterpriseInfo')!;
          assertEquals(
              expectedEnterpriseInfo,
              enterpriseInfoElement.textContent!.trim());
        }
        assertTrue(isChildVisible(app, '#proceed-button'));
        const proceedButton =
            app.shadowRoot!.querySelector<HTMLElement>('#proceed-button')!;
        assertEquals(expectedProceedLabel, proceedButton.textContent!.trim());
      }

      // Initial values.
      checkTextValues(
          useUpdatedUi ? app.i18n('profileDisclosureTitle') : 'title',
          useUpdatedUi ? app.i18n('profileDisclosureSubtitle') : 'subtitle',
          'enterprise_info',
          useUpdatedUi ? app.i18n('continueLabel') : 'proceed_label');
      checkImageUrl(targetElement, AVATAR_URL_1);
      assertFalse(isChildVisible(targetElement, '.work-badge'));

      // Update the values.
      webUIListenerCallback('on-profile-info-changed', {
        pictureUrl: AVATAR_URL_2,
        showEnterpriseBadge: true,
        title: 'new_title',
        subtitle: 'new_subtitle',
        enterpriseInfo: 'new_enterprise_info',
        proceedLabel: 'new_proceed_label',
        accountName: 'new_account_name',
        continueAs: 'new_continue_as',
        email: 'new_email@email.com',
        checkLinkDataCheckboxByDefault: false,
      });
      await microtasksFinished();

      checkTextValues(
          useUpdatedUi ? app.i18n('profileDisclosureTitle') : 'new_title',
          useUpdatedUi ? app.i18n('profileDisclosureSubtitle') : 'new_subtitle',
          'new_enterprise_info',
          useUpdatedUi ? app.i18n('continueLabel') : 'new_proceed_label');
      checkImageUrl(targetElement, AVATAR_URL_2);
      assertTrue(isChildVisible(targetElement, '.work-badge'));
      assertTrue(isChildVisible(app, '#cancel-button'));
    });

    test('onProfileInfoChangedValuePropositionSection', async function() {
      if (!useUpdatedUi) {
        return;
      }
      webUIListenerCallback('on-state-changed', State.VALUE_PROPOSITION);
      await microtasksFinished();

      const targetElement =
          app.shadowRoot!.querySelector<HTMLElement>('#value-prop')!;
      // Helper to test all the text values in the UI in the disclosure screenl.
      function checkValuePropositionTextValues(
          expectedTitle: string, expectedSubtitle: string,
          expectedEmail: string, expectedAccountName: string,
          expectedProceedLabel: string) {
        const target =
            app.shadowRoot!.querySelector<HTMLElement>('#value-prop')!;
        assertTrue(isChildVisible(target, '.title'));
        const titleElement =
            target.shadowRoot!.querySelector<HTMLElement>('.title')!;
        assertEquals(expectedTitle, titleElement.textContent!.trim());
        assertTrue(isChildVisible(target, '.subtitle'));
        const subtitleElement =
            target.shadowRoot!.querySelector<HTMLElement>('.subtitle')!;
        assertEquals(expectedSubtitle, subtitleElement.textContent!.trim());
        assertTrue(isChildVisible(target, '.email'));
        const emailElement =
            target.shadowRoot!.querySelector<HTMLElement>('.email')!;
        assertEquals(expectedEmail, emailElement.textContent!.trim());
        assertTrue(isChildVisible(target, '.account-name'));
        const accountNameElement =
            target.shadowRoot!.querySelector<HTMLElement>('.account-name')!;
        assertEquals(
            expectedAccountName, accountNameElement.textContent!.trim());
        assertTrue(isChildVisible(app, '#proceed-button'));
        const proceedButton =
            app.shadowRoot!.querySelector<HTMLElement>('#proceed-button')!;
        assertEquals(expectedProceedLabel, proceedButton.textContent!.trim());
      }

      // Initial values.
      checkValuePropositionTextValues(
          app.i18n('valuePropositionTitle'), app.i18n('valuePropSubtitle'),
          'email@email.com', 'account_name', 'continue_as');
      checkImageUrl(targetElement, AVATAR_URL_1);

      // Update the values.
      webUIListenerCallback('on-profile-info-changed', {
        pictureUrl: AVATAR_URL_2,
        showEnterpriseBadge: true,
        title: 'new_title',
        subtitle: 'new_subtitle',
        enterpriseInfo: 'new_enterprise_info',
        proceedLabel: 'new_proceed_label',
        accountName: 'new_account_name',
        continueAs: 'new_continue_as',
        email: 'new_email@email.com',
        checkLinkDataCheckboxByDefault: false,
      });
      await microtasksFinished();
      checkValuePropositionTextValues(
          app.i18n('valuePropositionTitle'), app.i18n('valuePropSubtitle'),
          'new_email@email.com', 'new_account_name', 'new_continue_as');

      checkImageUrl(targetElement, AVATAR_URL_2);
      assertTrue(isChildVisible(app, '#cancel-button'));
    });
  });
});

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://managed-user-profile-notice/legacy_managed_user_profile_notice_app.js';
import 'chrome://managed-user-profile-notice/managed_user_profile_notice_app.js';
import 'chrome://managed-user-profile-notice/managed_user_profile_notice_disclosure.js';

import type {LegacyManagedUserProfileNoticeAppElement} from 'chrome://managed-user-profile-notice/legacy_managed_user_profile_notice_app.js';
import type {ManagedUserProfileNoticeAppElement} from 'chrome://managed-user-profile-notice/managed_user_profile_notice_app.js';
import type {ManagedUserProfileInfo} from 'chrome://managed-user-profile-notice/managed_user_profile_notice_browser_proxy.js';
import {ManagedUserProfileNoticeBrowserProxyImpl} from 'chrome://managed-user-profile-notice/managed_user_profile_notice_browser_proxy.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

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
      showCancelButton: true,
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
      await waitAfterNextRender(app);
      return browserProxy.whenCalled('initialized');
    });

    teardown(function() {
      loadTimeData.overrideValues({'showLinkDataCheckbox': false});
    });

    /**
     * Checks that the expected image url is displayed.
     */
    function checkImageUrl(expectedUrl: string) {
      const targetElement = useUpdatedUi ?
          app.shadowRoot!.querySelector<HTMLElement>('#disclosure')! :
          app;
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
      await waitAfterNextRender(app);
      await browserProxy.whenCalled('initialized');

      assertTrue(isChildVisible(app, '#proceed-button'));
      assertTrue(isChildVisible(app, '#linkData'));

      const linkDataCheckbox: CrCheckboxElement =
          app.shadowRoot!.querySelector('#linkData')!;
      assertEquals(
          app.i18n('linkDataText'), linkDataCheckbox.textContent!.trim());
      assertFalse(linkDataCheckbox.checked);

      linkDataCheckbox.click();
      const proceedButton =
          app.shadowRoot!.querySelector<HTMLElement>('#proceed-button')!;

      await waitAfterNextRender(proceedButton);

      assertTrue(linkDataCheckbox.checked);
      assertEquals(
          app.i18n('proceedAlternateLabel'), proceedButton.textContent!.trim());
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
      await waitAfterNextRender(app);
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
        showCancelButton: true,
        checkLinkDataCheckboxByDefault: true,
      });

      const proceedButton =
          app.shadowRoot!.querySelector<HTMLElement>('#proceed-button')!;
      await waitAfterNextRender(proceedButton);

      assertTrue(linkDataCheckbox.checked);
      assertEquals(
          app.i18n('proceedAlternateLabel'), proceedButton.textContent!.trim());

      // We should be able to uncheck it.
      linkDataCheckbox.click();
      await waitAfterNextRender(proceedButton);
      assertEquals(
          app.i18n('linkDataText'), linkDataCheckbox.textContent!.trim());
      assertFalse(linkDataCheckbox.checked);
    });

    test('onProfileInfoChanged', function() {
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
      checkTextValues('title', 'subtitle', 'enterprise_info', 'proceed_label');
      checkImageUrl(AVATAR_URL_1);
      assertFalse(isChildVisible(targetElement, '.work-badge'));

      // Update the values.
      webUIListenerCallback('on-profile-info-changed', {
        pictureUrl: AVATAR_URL_2,
        showEnterpriseBadge: true,
        title: 'new_title',
        subtitle: 'new_subtitle',
        enterpriseInfo: 'new_enterprise_info',
        proceedLabel: 'new_proceed_label',
        showCancelButton: false,
        checkLinkDataCheckboxByDefault: false,
      });

      checkTextValues(
          'new_title', 'new_subtitle', 'new_enterprise_info',
          'new_proceed_label');
      checkImageUrl(AVATAR_URL_2);
      assertTrue(isChildVisible(targetElement, '.work-badge'));
      assertFalse(isChildVisible(app, '#cancel-button'));
    });
  });
});

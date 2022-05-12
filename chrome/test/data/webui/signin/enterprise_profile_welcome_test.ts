// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://enterprise-profile-welcome/enterprise_profile_welcome_app.js';

import {EnterpriseProfileWelcomeAppElement} from 'chrome://enterprise-profile-welcome/enterprise_profile_welcome_app.js';
import {EnterpriseProfileWelcomeBrowserProxyImpl} from 'chrome://enterprise-profile-welcome/enterprise_profile_welcome_browser_proxy.js';
import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {TestEnterpriseProfileWelcomeBrowserProxy} from './test_enterprise_profile_welcome_browser_proxy.js';

suite('EnterpriseProfileWelcomeTest', function() {
  let app: EnterpriseProfileWelcomeAppElement;
  let browserProxy: TestEnterpriseProfileWelcomeBrowserProxy;

  const AVATAR_URL_1: string = 'chrome://theme/IDR_PROFILE_AVATAR_1';
  const AVATAR_URL_2: string = 'chrome://theme/IDR_PROFILE_AVATAR_2';

  setup(async function() {
    browserProxy = new TestEnterpriseProfileWelcomeBrowserProxy({
      backgroundColor: 'rgb(255, 0, 0)',
      pictureUrl: AVATAR_URL_1,
      showEnterpriseBadge: false,
      title: 'title',
      subtitle: 'subtitle',
      enterpriseInfo: 'enterprise_info',
      proceedLabel: 'proceed_label',
      showCancelButton: true,
    });
    EnterpriseProfileWelcomeBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = '';
    app = document.createElement('enterprise-profile-welcome-app');
    document.body.appendChild(app);
    await waitAfterNextRender(app);
    return browserProxy.whenCalled('initialized');
  });

  /**
   * Checks that the expected image url is displayed.
   */
  function checkImageUrl(expectedUrl: string) {
    assertTrue(isChildVisible(app, '#avatar'));
    const img = app.shadowRoot!.querySelector<HTMLImageElement>('#avatar')!;
    assertEquals(expectedUrl, img.src);
  }

  test('proceed', async function() {
    assertTrue(isChildVisible(app, '#proceedButton'));
    app.$.proceedButton.click();
    await browserProxy.whenCalled('proceed');
  });

  test('cancel', async function() {
    assertTrue(isChildVisible(app, '#cancelButton'));
    app.$.cancelButton.click();
    await browserProxy.whenCalled('cancel');
  });

  test('linkData', async function() {
    assertTrue(isChildVisible(app, '#proceedButton'));
    assertFalse(isChildVisible(app, '#linkData'));

    loadTimeData.overrideValues({'showLinkDataCheckbox': true});

    document.body.innerHTML = '';
    app = document.createElement('enterprise-profile-welcome-app');
    document.body.appendChild(app);
    await waitAfterNextRender(app);
    await browserProxy.whenCalled('initialized');

    assertTrue(isChildVisible(app, '#proceedButton'));
    assertTrue(isChildVisible(app, '#linkData'));

    const linkDataCheckbox: CrCheckboxElement =
        app.shadowRoot!.querySelector('#linkData')!;
    assertEquals(
        app.i18n('linkDataText'), linkDataCheckbox.textContent!.trim());
    assertFalse(linkDataCheckbox.checked);

    linkDataCheckbox.click();

    await waitAfterNextRender(app.$.proceedButton);

    assertTrue(linkDataCheckbox.checked);
    assertEquals(
        app.i18n('proceedAlternateLabel'),
        app.$.proceedButton.textContent!.trim());
  });

  test('onProfileInfoChanged', function() {
    // Helper to test all the text values in the UI.
    function checkTextValues(
        expectedTitle: string, expectedSubtitle: string,
        expectedEnterpriseInfo: string, expectedProceedLabel: string) {
      assertTrue(isChildVisible(app, '#title'));
      const titleElement =
          app.shadowRoot!.querySelector<HTMLElement>('#title')!;
      assertEquals(expectedTitle, titleElement.textContent!.trim());
      assertTrue(isChildVisible(app, '#subtitle'));
      const subtitleElement =
          app.shadowRoot!.querySelector<HTMLElement>('#subtitle')!;
      assertEquals(expectedSubtitle, subtitleElement.textContent!.trim());
      assertTrue(isChildVisible(app, '#enterpriseInfo'));
      const enterpriseInfoElement =
          app.shadowRoot!.querySelector<HTMLElement>('#enterpriseInfo')!;
      assertEquals(
          expectedEnterpriseInfo, enterpriseInfoElement.textContent!.trim());
      assertTrue(isChildVisible(app, '#proceedButton'));
      const proceedButton = app.$.proceedButton;
      assertEquals(expectedProceedLabel, proceedButton.textContent!.trim());
    }

    // Initial values.
    checkTextValues('title', 'subtitle', 'enterprise_info', 'proceed_label');
    checkImageUrl(AVATAR_URL_1);
    assertFalse(isChildVisible(app, '.work-badge'));

    // Update the values.
    webUIListenerCallback('on-profile-info-changed', {
      backgroundColor: 'rgb(0, 255, 0)',
      pictureUrl: AVATAR_URL_2,
      showEnterpriseBadge: true,
      title: 'new_title',
      subtitle: 'new_subtitle',
      enterpriseInfo: 'new_enterprise_info',
      proceedLabel: 'new_proceed_label',
      showCancelButton: false
    });

    checkTextValues(
        'new_title', 'new_subtitle', 'new_enterprise_info',
        'new_proceed_label');
    checkImageUrl(AVATAR_URL_2);
    assertTrue(isChildVisible(app, '.work-badge'));
    assertFalse(isChildVisible(app, '#cancelButton'));
  });
});

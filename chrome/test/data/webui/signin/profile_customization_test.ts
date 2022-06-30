// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://profile-customization/profile_customization_app.js';

import {ProfileCustomizationAppElement} from 'chrome://profile-customization/profile_customization_app.js';
import {ProfileCustomizationBrowserProxyImpl} from 'chrome://profile-customization/profile_customization_browser_proxy.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

import {TestProfileCustomizationBrowserProxy} from './test_profile_customization_browser_proxy.js';

[true, false].forEach(inDialogDesign => {
  const inDialogDesignSuffix = inDialogDesign ? 'InDialogDesign' : '';

  suite(`ProfileCustomizationTest${inDialogDesignSuffix}`, function() {
    let app: ProfileCustomizationAppElement;
    let browserProxy: TestProfileCustomizationBrowserProxy;

    const AVATAR_URL_1 = 'chrome://theme/IDR_PROFILE_AVATAR_1';
    const AVATAR_URL_2 = 'chrome://theme/IDR_PROFILE_AVATAR_2';
    const WELCOME_TEXT_1 = 'Welcome, Bob';
    const WELCOME_TEXT_2 = 'Hi, Elisa';
    const STATIC_TITLE_PATTERN = /^Customize your (Chromium|Chrome) profile$/g;

    setup(function() {
      loadTimeData.overrideValues({
        profileName: 'TestName',
        profileCustomizationInDialogDesign: inDialogDesign,
      });
      browserProxy = new TestProfileCustomizationBrowserProxy();
      browserProxy.setProfileInfo({
        backgroundColor: 'rgb(0, 255, 0)',
        pictureUrl: AVATAR_URL_1,
        isManaged: false,
        welcomeTitle: WELCOME_TEXT_1,
      });
      ProfileCustomizationBrowserProxyImpl.setInstance(browserProxy);
      document.body.innerHTML = '';
      app = document.createElement('profile-customization-app');
      document.body.append(app);
      return browserProxy.whenCalled('initialized');
    });

    function checkImageUrl(elementId: string, expectedUrl: string) {
      assertTrue(isChildVisible(app, elementId));
      const img = app.shadowRoot!.querySelector<HTMLImageElement>(elementId)!;
      assertEquals(expectedUrl, img.src);
    }

    // Checks that clicking Done without interacting with the input does not
    // change the name.
    test('ClickDone', function() {
      assertTrue(isChildVisible(app, '#doneButton'));
      const doneButton = app.$.doneButton;
      assertFalse(doneButton.disabled);
      doneButton.click();
      return browserProxy.whenCalled('done').then(
          profileName => assertEquals('TestName', profileName));
    });

    // Checks that the name can be changed.
    test('ChangeName', function() {
      const nameInput = app.$.nameInput;
      // Check the default value for the input.
      assertEquals('TestName', nameInput.value);
      assertFalse(nameInput.invalid);

      // Invalid name (white space).
      nameInput.value = '   ';
      assertTrue(nameInput.invalid);

      // The button is disabled.
      assertTrue(isChildVisible(app, '#doneButton'));
      const doneButton = app.$.doneButton;
      assertTrue(doneButton.disabled);

      // Empty name.
      nameInput.value = '';
      assertTrue(nameInput.invalid);
      assertTrue(doneButton.disabled);

      // Valid name.
      nameInput.value = 'Bob';
      assertFalse(nameInput.invalid);

      // Click done, and check that the new name is sent.
      assertTrue(isChildVisible(app, '#doneButton'));
      assertFalse(doneButton.disabled);
      doneButton.click();
      return browserProxy.whenCalled('done').then(
          profileName => assertEquals('Bob', profileName));
    });

    test('ProfileInfo', function() {
      const header = app.shadowRoot!.querySelector<HTMLElement>('#header')!;
      // Check initial info.
      if (inDialogDesign) {
        assertTrue(app.$.title.innerText.match(STATIC_TITLE_PATTERN) != null);
      } else {
        assertEquals(app.$.title.innerText, WELCOME_TEXT_1);
      }
      assertEquals('rgb(0, 255, 0)', getComputedStyle(header).backgroundColor);
      checkImageUrl('#avatar', AVATAR_URL_1);
      assertFalse(isChildVisible(app, '#badge'));
      // Update the info.
      const color2 = 'rgb(4, 5, 6)';
      webUIListenerCallback('on-profile-info-changed', {
        backgroundColor: color2,
        pictureUrl: AVATAR_URL_2,
        isManaged: true,
        welcomeTitle: WELCOME_TEXT_2,
      });
      if (inDialogDesign) {
        assertTrue(app.$.title.innerText.match(STATIC_TITLE_PATTERN) != null);
      } else {
        assertEquals(app.$.title.innerText, WELCOME_TEXT_2);
      }
      assertEquals(color2, getComputedStyle(header).backgroundColor);
      checkImageUrl('#avatar', AVATAR_URL_2);
      assertTrue(isChildVisible(app, '#badge'));
    });

    test('ThemeSelector', function() {
      assertTrue(!!app.shadowRoot!.querySelector('#themeSelector'));
    });

    // Checks that the Skip button is present when the page is displayed in a
    // dialog in Sync Promo
    test('HasSkipButton', function() {
      assertEquals(inDialogDesign, isChildVisible(app, '#skipButton'));
    });
  });
});

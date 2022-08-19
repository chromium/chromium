// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender} from 'chrome://test/test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

let personalizationPage = null;

function createPersonalizationPage() {
  PolymerTest.clearBody();

  personalizationPage = document.createElement('settings-personalization-page');
  personalizationPage.set('prefs', {
    extensions: {
      theme: {
        id: {
          value: '',
        },
        use_system: {
          value: false,
        },
      },
    },
  });

  personalizationPage.set('pageVisibility', {
    setWallpaper: true,
  });

  document.body.appendChild(personalizationPage);
  flush();
}

suite('PersonalizationHandler', function() {
  suiteSetup(function() {
    assertFalse(
        loadTimeData.getBoolean('isPersonalizationHubEnabled'),
        'this test should only run with PersonalizationHub disabled');
    testing.Test.disableAnimationsAndTransitions();
  });

  setup(function() {
    createPersonalizationPage();
  });

  teardown(function() {
    personalizationPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('changePicture', function() {
    const row =
        personalizationPage.shadowRoot.getElementById('changePictureRow');
    assertTrue(!!row);
    row.click();
    assertEquals(routes.CHANGE_PICTURE, Router.getInstance().getCurrentRoute());
  });

  test('Deep link to change account picture', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '503');
    Router.getInstance().navigateTo(routes.CHANGE_PICTURE, params);

    flush();

    await waitAfterNextRender(personalizationPage);

    const changePicturePage =
        personalizationPage.shadowRoot.querySelector('settings-change-picture');
    assertTrue(!!changePicturePage);
    const deepLinkElement =
        changePicturePage.shadowRoot.querySelector('#pictureList')
            .shadowRoot.querySelector('#selector')
            .$$('[class="iron-selected"]');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Account picture elem should be focused for settingId=503.');
  });
});

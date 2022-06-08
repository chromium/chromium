// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender} from 'chrome://test/test_util.js';

import {assertEquals} from '../../chai_assert.js';

suite('PrivacyHubSubpageTests', function() {
  /** @type {SettingsPrivacyHubPage} */
  let privacyHubSubpage = null;

  setup(async () => {
    loadTimeData.overrideValues({
      showPrivacyHub: true,
    });

    PolymerTest.clearBody();
    privacyHubSubpage = document.createElement('settings-privacy-hub-page');
    document.body.appendChild(privacyHubSubpage);
  });

  teardown(function() {
    privacyHubSubpage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to camera toggle on privacy hub', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1116');
    Router.getInstance().navigateTo(routes.PRIVACY_HUB, params);

    flush();

    const deepLinkElement =
        privacyHubSubpage.shadowRoot.querySelector('#cameraToggle')
            .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Camera toggle should be focused for settingId=1116.');
  });
});
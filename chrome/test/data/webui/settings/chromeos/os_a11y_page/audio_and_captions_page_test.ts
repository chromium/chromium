// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsAudioAndCaptionsPageElement} from 'chrome://os-settings/lazy_load.js';
import {Router, routes} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('<settings-audio-and-captions-page>', () => {
  let page: SettingsAudioAndCaptionsPageElement|null = null;

  function initPage() {
    page = document.createElement('settings-audio-and-captions-page');
    document.body.appendChild(page);
    flush();
  }

  setup(() => {
    Router.getInstance().navigateTo(routes.A11Y_AUDIO_AND_CAPTIONS);
  });

  teardown(() => {
    page!.remove();
    page = null;
    Router.getInstance().resetRouteForTesting();
  });

  test('no subpages are available in kiosk mode', () => {
    loadTimeData.overrideValues({
      isKioskModeActive: true,
      showTabletModeShelfNavigationButtonsSettings: true,
    });
    initPage();

    const subpageLinks = page!.shadowRoot!.querySelectorAll('cr-link-row');
    subpageLinks.forEach(subpageLink => assertFalse(isVisible(subpageLink)));
  });
});

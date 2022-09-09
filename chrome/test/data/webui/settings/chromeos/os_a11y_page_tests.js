// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertEquals} from '../../chai_assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {waitAfterNextRender} from 'chrome://test/test_util.js';

suite('A11yPageTests', function() {
  /** @type {SettingsA11yPageElement} */
  let a11yPage = null;

  setup(function() {
    PolymerTest.clearBody();
    a11yPage = document.createElement('os-settings-a11y-page');
    document.body.appendChild(a11yPage);
    flush();
  });

  teardown(function() {
    a11yPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to always show a11y settings', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1500');
    Router.getInstance().navigateTo(
        routes.OS_ACCESSIBILITY, params);

    flush();

    const deepLinkElement =
        a11yPage.shadowRoot.querySelector('#optionsInMenuToggle')
            .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Always show a11y toggle should be focused for settingId=1500.');
  });
});

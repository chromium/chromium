// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('OSSearchPageTests', function() {
  /** @type {?SettingsSearchPageElement} */
  let page = null;

  setup(function() {
    loadTimeData.overrideValues({
      shouldShowQuickAnswersSettings: false,
    });
    page = document.createElement('os-settings-search-page');
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to preferred search engine', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '600');
    Router.getInstance().navigateTo(routes.OS_SEARCH, params);

    const browserSearchSettingsLink =
        page.shadowRoot.querySelector('settings-search-engine')
            .shadowRoot.querySelector('#browserSearchSettingsLink');
    const deepLinkElement =
        browserSearchSettingsLink.shadowRoot.querySelector('cr-icon-button');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Preferred search dropdown should be focused for settingId=600.');
  });
});

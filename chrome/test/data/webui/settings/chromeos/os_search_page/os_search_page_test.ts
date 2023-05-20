// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OsSettingsSearchPageElement, Router, routes} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util_ts.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

suite('<os-settings-search-page>', () => {
  let page: OsSettingsSearchPageElement;

  setup(() => {
    loadTimeData.overrideValues({
      shouldShowQuickAnswersSettings: false,
    });
    page = document.createElement('os-settings-search-page');
    document.body.appendChild(page);
    flush();
  });

  teardown(() => {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to preferred search engine', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '600');
    Router.getInstance().navigateTo(routes.OS_SEARCH, params);

    const settingsSearchEngine =
        page.shadowRoot!.querySelector('settings-search-engine');
    assertTrue(!!settingsSearchEngine);

    const browserSearchSettingsLink =
        settingsSearchEngine.shadowRoot!.querySelector(
            '#browserSearchSettingsLink');
    assertTrue(!!browserSearchSettingsLink);

    const deepLinkElement =
        browserSearchSettingsLink.shadowRoot!.querySelector('cr-icon-button');
    assertTrue(!!deepLinkElement);

    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Preferred search dropdown should be focused for settingId=600.');
  });
});

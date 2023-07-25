// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OsSettingsRoutes, OsSettingsSearchPageElement, Router, routes} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util_ts.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

interface SubpageTriggerData {
  triggerSelector: string;
  routeName: keyof OsSettingsRoutes;
}

suite('<os-settings-search-page>', () => {
  let page: OsSettingsSearchPageElement;

  function createPage() {
    page = document.createElement('os-settings-search-page');
    document.body.appendChild(page);
    flush();
  }

  teardown(() => {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to preferred search engine', async () => {
    loadTimeData.overrideValues({
      shouldShowQuickAnswersSettings: false,
    });
    createPage();

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

  const subpageTriggerData: SubpageTriggerData[] = [
    {
      triggerSelector: '#searchRow',
      routeName: 'SEARCH_SUBPAGE',
    },
    {
      triggerSelector: '#assistantRow',
      routeName: 'GOOGLE_ASSISTANT',
    },
  ];
  subpageTriggerData.forEach(({triggerSelector, routeName}) => {
    test(
        `Row for ${routeName} is focused when returning from subpage`,
        async () => {
          loadTimeData.overrideValues({
            isAssistantAllowed: true,              // Show google assistant row
            shouldShowQuickAnswersSettings: true,  // Show quick answers row
          });
          createPage();

          Router.getInstance().navigateTo(routes.OS_SEARCH);

          const subpageTrigger =
              page.shadowRoot!.querySelector<HTMLElement>(triggerSelector);
          assertTrue(!!subpageTrigger);

          // Sub-page trigger navigates to subpage for route
          subpageTrigger.click();
          assertEquals(routes[routeName], Router.getInstance().currentRoute);

          // Navigate back
          const popStateEventPromise = eventToPromise('popstate', window);
          Router.getInstance().navigateToPreviousRoute();
          await popStateEventPromise;
          await waitAfterNextRender(page);

          assertEquals(
              subpageTrigger, page.shadowRoot!.activeElement,
              `${triggerSelector} should be focused.`);
        });
  });
});

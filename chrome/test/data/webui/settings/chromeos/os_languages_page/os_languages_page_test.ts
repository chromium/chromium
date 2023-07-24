// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {OsSettingsLanguagesSectionElement, OsSettingsRoutes, Router, routes} from 'chrome://os-settings/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

interface SubpageTriggerData {
  triggerSelector: string;
  routeName: keyof OsSettingsRoutes;
}

suite('<os-settings-languages-section>', () => {
  let page: OsSettingsLanguagesSectionElement;

  function createPage() {
    page = document.createElement('os-settings-languages-section');
    document.body.appendChild(page);
    flush();
  }

  teardown(() => {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  const subpageTriggerData: SubpageTriggerData[] = [
    {
      triggerSelector: '#languagesRow',
      routeName: 'OS_LANGUAGES_LANGUAGES',
    },
    {
      triggerSelector: '#inputRow',
      routeName: 'OS_LANGUAGES_INPUT',
    },
    {
      triggerSelector: '#smartInputsRow',
      routeName: 'OS_LANGUAGES_SMART_INPUTS',
    },
  ];
  subpageTriggerData.forEach(({triggerSelector, routeName}) => {
    test(
        `Row for ${routeName} is focused when returning from subpage`,
        async () => {
          Router.getInstance().navigateTo(routes.OS_LANGUAGES);
          createPage();

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

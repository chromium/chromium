// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {LanguageSettingsCardElement} from 'chrome://os-settings/lazy_load.js';
import {OsSettingsRoutes, Router, routes} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

interface SubpageTriggerData {
  triggerSelector: string;
  routeName: keyof OsSettingsRoutes;
}

suite('<language-settings-card>', () => {
  let languageSettingsCard: LanguageSettingsCardElement;

  function createLanguagesCard(): void {
    languageSettingsCard = document.createElement('language-settings-card');
    document.body.appendChild(languageSettingsCard);
    flush();
  }

  setup(() => {
    loadTimeData.overrideValues({allowEmojiSuggestion: true});
  });

  teardown(() => {
    languageSettingsCard.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Smart inputs row is visible if smart inputs are enabled', () => {
    loadTimeData.overrideValues({allowEmojiSuggestion: true});
    createLanguagesCard();
    const smartInputsRow =
        languageSettingsCard.shadowRoot!.querySelector<HTMLElement>(
            '#smartInputsRow');
    assertTrue(isVisible(smartInputsRow));
  });

  test('Smart inputs row is hidden if smart inputs are disabled', () => {
    loadTimeData.overrideValues({allowEmojiSuggestion: false});
    createLanguagesCard();
    const smartInputsRow =
        languageSettingsCard.shadowRoot!.querySelector<HTMLElement>(
            '#smartInputsRow');
    assertFalse(isVisible(smartInputsRow));
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
          const languageSettingsCardRoute = languageSettingsCard.route;
          assertTrue(!!languageSettingsCardRoute);
          Router.getInstance().navigateTo(languageSettingsCardRoute);

          createLanguagesCard();

          const subpageTrigger =
              languageSettingsCard.shadowRoot!.querySelector<HTMLElement>(
                  triggerSelector);
          assertTrue(!!subpageTrigger);

          // Sub-page trigger navigates to subpage for route
          subpageTrigger.click();
          assertEquals(routes[routeName], Router.getInstance().currentRoute);

          // Navigate back
          const popStateEventPromise = eventToPromise('popstate', window);
          Router.getInstance().navigateToPreviousRoute();
          await popStateEventPromise;
          await waitAfterNextRender(languageSettingsCard);

          assertEquals(
              subpageTrigger, languageSettingsCard.shadowRoot!.activeElement,
              `${triggerSelector} should be focused.`);
        });
  });
});

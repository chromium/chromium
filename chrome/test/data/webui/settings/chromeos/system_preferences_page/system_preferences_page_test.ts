// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Browser tests for the System preferences page.
 *
 * - This suite expects the OsSettingsRevampWayfinding feature flag to be
 *   enabled.
 */

import 'chrome://os-settings/os_settings.js';

import {ensureLazyLoaded, OsSettingsRoutes, OsSettingsSubpageElement, resetGlobalScrollTargetForTesting, Route, Router, routes, setGlobalScrollTargetForTesting, SettingsSystemPreferencesPageElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

interface SubpageData {
  routeName: keyof OsSettingsRoutes;
  elementTagName: string;
}

suite('<settings-system-preferences-page>', () => {
  let page: SettingsSystemPreferencesPageElement;

  async function createPage() {
    page = document.createElement('settings-system-preferences-page');
    document.body.appendChild(page);
    await flushTasks();
  }

  async function navigateToSubpage(route: Route) {
    await ensureLazyLoaded();
    Router.getInstance().navigateTo(route);
    await flushTasks();
  }

  /**
   * Expects the os-settings-subpage parent element containing the subpage
   * element with the given tag name to be visible on the page.
   */
  function assertSubpageIsVisible(elementTagName: string) {
    const subpageElement = page.shadowRoot!.querySelector(elementTagName);
    assertTrue(!!subpageElement);
    const subpageParentElement = subpageElement.parentNode as HTMLElement;
    assertTrue(subpageParentElement instanceof OsSettingsSubpageElement);
    assertTrue(
        isVisible(subpageParentElement),
        `${elementTagName} should be visible.`);
  }

  setup(() => {
    Router.getInstance().navigateTo(routes.SYSTEM_PREFERENCES);
  });

  teardown(() => {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  suite('Date and Time subsection', () => {
    test('Date and Time settings card is visible', async () => {
      await createPage();

      const dateTimeSettingsCard =
          page.shadowRoot!.querySelector('date-time-settings-card');
      assertTrue(
          isVisible(dateTimeSettingsCard),
          'Date and Time settings card should be visible.');
    });

    test('Timezone subpage is visible', async () => {
      await createPage();

      await navigateToSubpage(routes.DATETIME_TIMEZONE_SUBPAGE);
      assertSubpageIsVisible('timezone-subpage');
    });
  });

  suite('Languages and Input subsection', () => {
    setup(() => {
      // Necessary for os-settings-edit-dictionary-page which uses
      // GlobalScrollTargetMixin
      setGlobalScrollTargetForTesting(document.body);
    });

    teardown(() => {
      resetGlobalScrollTargetForTesting();
    });

    test('Language settings card is visible', async () => {
      await createPage();

      const languageSettingsCard =
          page.shadowRoot!.querySelector('language-settings-card');
      assertTrue(
          isVisible(languageSettingsCard),
          'Language settings card should be visible.');
    });

    const languageSubpages: SubpageData[] = [
      {
        routeName: 'OS_LANGUAGES_LANGUAGES',
        elementTagName: 'os-settings-languages-page-v2',
      },
      {
        routeName: 'OS_LANGUAGES_INPUT',
        elementTagName: 'os-settings-input-page',
      },
      {
        routeName: 'OS_LANGUAGES_INPUT_METHOD_OPTIONS',
        elementTagName: 'settings-input-method-options-page',
      },
      {
        routeName: 'OS_LANGUAGES_SMART_INPUTS',
        elementTagName: 'os-settings-smart-inputs-page',
      },
      {
        routeName: 'OS_LANGUAGES_EDIT_DICTIONARY',
        elementTagName: 'os-settings-edit-dictionary-page',
      },
      {
        routeName: 'OS_LANGUAGES_JAPANESE_MANAGE_USER_DICTIONARY',
        elementTagName: 'os-settings-japanese-manage-user-dictionary-page',
      },
    ];
    languageSubpages.forEach(({routeName, elementTagName}) => {
      test(
          `${elementTagName} subpage element is visible for route ${routeName}`,
          async () => {
            await createPage();

            await navigateToSubpage(routes[routeName]);
            assertSubpageIsVisible(elementTagName);
          });
    });
  });

  suite('Reset subsection', () => {
    test('Reset settings card is visible if powerwash is allowed', async () => {
      loadTimeData.overrideValues({allowPowerwash: true});
      await createPage();

      const resetSettingsCard =
          page.shadowRoot!.querySelector('reset-settings-card');
      assertTrue(
          isVisible(resetSettingsCard),
          'Reset settings card should be visible.');
    });

    test(
        'Reset settings card is not visible if powerwash is disallowed',
        async () => {
          loadTimeData.overrideValues({allowPowerwash: false});
          await createPage();

          const resetSettingsCard =
              page.shadowRoot!.querySelector('reset-settings-card');
          assertFalse(
              isVisible(resetSettingsCard),
              'Reset settings card should not be visible.');
        });
  });

  suite('Search & Assistant subsection', () => {
    test('Search and Assistant settings card is visible', async () => {
      await createPage();

      const searchAndAssistantSettingsCard =
          page.shadowRoot!.querySelector('search-and-assistant-settings-card');
      assertTrue(
          isVisible(searchAndAssistantSettingsCard),
          'Search and Assistant settings card should be visible.');
    });

    test('Search subpage is visible if quick answers is enabled', async () => {
      loadTimeData.overrideValues({shouldShowQuickAnswersSettings: true});
      await createPage();

      await navigateToSubpage(routes.SEARCH_SUBPAGE);
      assertSubpageIsVisible('settings-search-subpage');
    });

    test(
        'Search subpage is not stamped if quick answers is disabled',
        async () => {
          loadTimeData.overrideValues({shouldShowQuickAnswersSettings: false});
          await createPage();

          await navigateToSubpage(routes.SEARCH_SUBPAGE);
          const subpage =
              page.shadowRoot!.querySelector('settings-search-subpage');
          assertNull(subpage, 'Subpage should not be stamped.');
        });

    test('Assistant subpage is visible if assistant is enabled', async () => {
      loadTimeData.overrideValues({isAssistantAllowed: true});
      await createPage();

      await navigateToSubpage(routes.GOOGLE_ASSISTANT);
      assertSubpageIsVisible('settings-google-assistant-subpage');
    });

    test(
        'Assistant subpage is not stamped if assistant is disabled',
        async () => {
          loadTimeData.overrideValues({isAssistantAllowed: false});
          await createPage();

          await navigateToSubpage(routes.GOOGLE_ASSISTANT);
          const subpage = page.shadowRoot!.querySelector(
              'settings-google-assistant-subpage');
          assertNull(subpage, 'Subpage should not be stamped.');
        });
  });
});

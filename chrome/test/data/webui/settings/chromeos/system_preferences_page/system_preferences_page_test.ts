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

import {ensureLazyLoaded, Route, Router, routes, SettingsSystemPreferencesPageElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

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

  teardown(() => {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  suite('Reset subsection', () => {
    test('Reset card is visible if powerwash is allowed', async () => {
      loadTimeData.overrideValues({allowPowerwash: true});
      Router.getInstance().navigateTo(routes.SYSTEM_PREFERENCES);
      await createPage();

      const resetCard = page.shadowRoot!.querySelector('settings-reset-card');
      assertTrue(isVisible(resetCard), 'Reset card should be visible.');
    });

    test('Reset card is not visible if powerwash is disallowed', async () => {
      loadTimeData.overrideValues({allowPowerwash: false});
      Router.getInstance().navigateTo(routes.SYSTEM_PREFERENCES);
      await createPage();

      const resetCard = page.shadowRoot!.querySelector('settings-reset-card');
      assertFalse(isVisible(resetCard), 'Reset card should not be visible.');
    });
  });

  suite('Search & Assistant subsection', () => {
    test('Search and Assistant card is always visible', async () => {
      Router.getInstance().navigateTo(routes.SYSTEM_PREFERENCES);
      await createPage();

      const card =
          page.shadowRoot!.querySelector('settings-search-and-assistant-card');
      assertTrue(
          isVisible(card), 'Search and Assistant card should be visible.');
    });

    test(
        'Search subpage should be visible if quick answers is enabled',
        async () => {
          loadTimeData.overrideValues({shouldShowQuickAnswersSettings: true});
          await createPage();

          await navigateToSubpage(routes.SEARCH_SUBPAGE);
          const subpage =
              page.shadowRoot!.querySelector('settings-search-subpage');
          assertTrue(isVisible(subpage), 'Subpage should be visible.');
        });

    test(
        'Search subpage should not be stamped if quick answers is disabled',
        async () => {
          loadTimeData.overrideValues({shouldShowQuickAnswersSettings: false});
          await createPage();

          await navigateToSubpage(routes.SEARCH_SUBPAGE);
          const subpage =
              page.shadowRoot!.querySelector('settings-search-subpage');
          assertNull(subpage, 'Subpage should not be stamped.');
        });

    test(
        'Assistant subpage should be visible if assistant is enabled',
        async () => {
          loadTimeData.overrideValues({isAssistantAllowed: true});
          await createPage();

          await navigateToSubpage(routes.GOOGLE_ASSISTANT);
          const subpage = page.shadowRoot!.querySelector(
              'settings-google-assistant-subpage');
          assertTrue(isVisible(subpage), 'Subpage should be visible.');
        });

    test(
        'Assistant subpage should not be stamped if assistant is disabled',
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

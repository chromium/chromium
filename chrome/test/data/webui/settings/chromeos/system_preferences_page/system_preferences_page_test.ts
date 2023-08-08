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

import {Router, routes, SettingsSystemPreferencesPageElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('<settings-system-preferences-page>', () => {
  let page: SettingsSystemPreferencesPageElement;

  async function createPage() {
    page = document.createElement('settings-system-preferences-page');
    page.prefs = {};
    document.body.appendChild(page);
    await flushTasks();
  }

  teardown(() => {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

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

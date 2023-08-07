// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {Router, routes, SettingsSystemPreferencesPageElement} from 'chrome://os-settings/os_settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
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

  test('Page is created and visible without errors', async () => {
    Router.getInstance().navigateTo(routes.SYSTEM_PREFERENCES);
    await createPage();
    assertTrue(isVisible(page));
  });
});

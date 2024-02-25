// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OsSettingsSearchPageElement, Router, routes} from 'chrome://os-settings/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

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

  test('Search and Assistant settings card should be visible', async () => {
    createPage();

    Router.getInstance().navigateTo(routes.OS_SEARCH);
    await flushTasks();

    const searchAndAssistantSettingsCard =
        page.shadowRoot!.querySelector('search-and-assistant-settings-card');
    assertTrue(isVisible(searchAndAssistantSettingsCard));
  });
});

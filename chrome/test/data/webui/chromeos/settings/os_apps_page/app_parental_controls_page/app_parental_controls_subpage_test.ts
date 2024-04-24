// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsAppParentalControlsSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {appParentalControlsHandlerMojom, Router, routes, setAppParentalControlsProviderForTesting} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakeAppParentalControlsHandler} from './fake_app_parental_controls_handler.js';

suite('AppParentalControlsPageTest', () => {
  let page: SettingsAppParentalControlsSubpageElement;
  let handler: FakeAppParentalControlsHandler;

  async function createPage(): Promise<void> {
    Router.getInstance().navigateTo(routes.APP_PARENTAL_CONTROLS);
    page = new SettingsAppParentalControlsSubpageElement();
    document.body.appendChild(page);
    await flushTasks();
  }

  suiteSetup(() => {
    handler = new FakeAppParentalControlsHandler();
    setAppParentalControlsProviderForTesting(handler);
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
  });

  function createApp(
      id: string, title: string): appParentalControlsHandlerMojom.App {
    return {id, title};
  }

  test('App list is in alphabetical order', async () => {
    const appTitle1 = 'Files';
    const appTitle2 = 'Chrome';
    handler.addAppForTesting(createApp('file-id', appTitle1));
    handler.addAppForTesting(createApp('chrome-id', appTitle2));

    await createPage();

    const appList = page.shadowRoot!.querySelector('#appParentalControlsList');
    assertTrue(!!appList);
    assertTrue(isVisible(appList));

    const apps = appList.querySelectorAll<HTMLElement>('.cr-row');
    assertEquals(2, apps.length);
    assertTrue(!!apps[0]);
    assertTrue(!!apps[1]);

    assertEquals(apps[0].innerText, appTitle2);
    assertEquals(apps[1].innerText, appTitle1);
  });
});

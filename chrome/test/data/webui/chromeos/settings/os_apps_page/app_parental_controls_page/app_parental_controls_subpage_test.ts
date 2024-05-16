// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsAppParentalControlsSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {Router, routes, setAppParentalControlsProviderForTesting} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakeAppParentalControlsHandler} from './fake_app_parental_controls_handler.js';
import {createApp} from './test_utils.js';

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

  test('App list is in alphabetical order', async () => {
    const appTitle1 = 'Files';
    const appTitle2 = 'Chrome';
    handler.addAppForTesting(createApp('file-id', appTitle1, false));
    handler.addAppForTesting(createApp('chrome-id', appTitle2, false));

    await createPage();

    const appList = page.shadowRoot!.querySelector('#appParentalControlsList');
    assertTrue(!!appList);
    assertTrue(isVisible(appList));

    const apps = appList.querySelectorAll<HTMLElement>('block-app-item');
    assertEquals(2, apps.length);
    assertTrue(!!apps[0]);
    assertTrue(!!apps[1]);

    const title1 = apps[0].shadowRoot!.querySelector<HTMLElement>('.app-title');
    assertTrue(!!title1);
    assertEquals(title1.innerText, appTitle2);

    const title2 = apps[1].shadowRoot!.querySelector<HTMLElement>('.app-title');
    assertTrue(!!title2);
    assertEquals(title2.innerText, appTitle1);
  });

});

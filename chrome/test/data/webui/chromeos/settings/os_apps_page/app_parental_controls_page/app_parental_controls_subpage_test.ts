// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsAppParentalControlsSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {CrToggleElement, Router, routes, setAppParentalControlsProviderForTesting} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
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

  setup(() => {
    handler = new FakeAppParentalControlsHandler();
    setAppParentalControlsProviderForTesting(handler);
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
    page.remove();
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

  test('Click toggle updates app blocked state', async () => {
    const app = createApp('file-id', 'Files', false);
    handler.addAppForTesting(app);

    await createPage();

    const appList = page.shadowRoot!.querySelector('#appParentalControlsList');
    assertTrue(!!appList);
    assertTrue(isVisible(appList));

    const apps = appList.querySelectorAll<HTMLElement>('block-app-item');
    assertEquals(1, apps.length);

    const appElement = apps[0];
    assertTrue(!!appElement);
    const appToggle =
        appElement.shadowRoot!.querySelector<CrToggleElement>('.app-toggle');
    assertTrue(!!appToggle);
    assertTrue(appToggle.checked);

    appToggle.click();
    assertFalse(appToggle.checked);

    assertEquals(handler.getCallCount('updateApp'), 1);
    const args1 = handler.getArgs('updateApp')[0];
    assertEquals(2, args1.length);
    assertEquals(app.id, args1[0]);
    assertTrue(/*isBlocked*/ args1[1]);
    assertTrue(app.isBlocked);

    appToggle.click();
    assertTrue(appToggle.checked);

    assertEquals(handler.getCallCount('updateApp'), 2);
    const args2 = handler.getArgs('updateApp')[1];
    assertEquals(2, args2.length);
    assertEquals(app.id, args2[0]);
    assertFalse(/*isBlocked*/ args2[1]);
    assertFalse(app.isBlocked);
  });

  test(
      'Readiness events without block state changes do not update list',
      async () => {
        const app = createApp('file-id', 'Files', false);
        handler.addAppForTesting(app);

        await createPage();

        const observer = handler.getObserverRemote();
        assertTrue(!!observer);

        const appList =
            page.shadowRoot!.querySelector('#appParentalControlsList');
        assertTrue(!!appList);
        assertTrue(isVisible(appList));

        const apps = appList.querySelectorAll<HTMLElement>('block-app-item');
        assertEquals(1, apps.length);

        const appElement = apps[0];
        assertTrue(!!appElement);
        const appToggle = appElement.shadowRoot!.querySelector<CrToggleElement>(
            '.app-toggle');
        assertTrue(!!appToggle);
        assertTrue(appToggle.checked);

        // Dispatch readiness changed event without any changes to block state.
        observer.onReadinessChanged(app);

        assertTrue(appToggle.checked);
        assertFalse(app.isBlocked);
      });
});

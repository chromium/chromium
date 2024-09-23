// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {BlockAppItemElement} from 'chrome://os-settings/lazy_load.js';
import {appParentalControlsHandlerMojom, CrToggleElement, setAppParentalControlsProviderForTesting} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakeAppParentalControlsHandler} from './fake_app_parental_controls_handler.js';
import {createApp} from './test_utils.js';

suite('BlockAppItemElementTest', () => {
  let blockAppItem: BlockAppItemElement;
  let app: appParentalControlsHandlerMojom.App;
  let handler: FakeAppParentalControlsHandler;

  async function createBlockAppItem(isAvailable: boolean = true):
      Promise<void> {
    blockAppItem = document.createElement('block-app-item');

    app = createApp('test-app', 'TestApp', !isAvailable);
    handler.addAppForTesting(app);
    blockAppItem.app = app;

    document.body.appendChild(blockAppItem);
    await flushTasks();
  }

  function getAppToggle(): CrToggleElement|null {
    return blockAppItem.shadowRoot!.querySelector<CrToggleElement>(
        '.app-toggle');
  }

  setup(async () => {
    handler = new FakeAppParentalControlsHandler();
    setAppParentalControlsProviderForTesting(handler);
  });

  teardown(() => {
    blockAppItem.remove();
  });


  test('UI elements are shown', async () => {
    await createBlockAppItem();

    const appTitle =
        blockAppItem.shadowRoot!.querySelector<HTMLElement>('.app-title');
    assertTrue(!!appTitle);
    assertTrue(isVisible(appTitle));
    assertEquals(app.title, appTitle.innerText);

    const appIcon =
        blockAppItem.shadowRoot!.querySelector<HTMLElement>('.app-icon');
    assertTrue(!!appIcon);
    assertTrue(isVisible(appIcon));

    const appToggle = getAppToggle();
    assertTrue(!!appToggle);
    assertTrue(isVisible(appToggle));
  });

  test('App toggle is initially checked for available app', async () => {
    await createBlockAppItem();

    const appToggle = getAppToggle();
    assertTrue(!!appToggle);
    assertTrue(appToggle.checked);
  });

  test('App toggle is initially unchecked for blocked app', async () => {
    await createBlockAppItem(false);

    const appToggle = getAppToggle();
    assertTrue(!!appToggle);
    assertFalse(appToggle.checked);
  });

  test('Updating app object updates UI', async () => {
    await createBlockAppItem();

    const appToggle = getAppToggle();
    assertTrue(!!appToggle);
    assertTrue(appToggle.checked);

    const updatedApp = {
      id: app.id,
      title: app.title,
      isBlocked: !app.isBlocked,
    };
    blockAppItem.app = updatedApp;

    assertFalse(appToggle.checked);
  });
});

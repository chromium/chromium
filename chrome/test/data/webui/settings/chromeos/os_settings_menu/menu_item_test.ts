// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {OsSettingsMenuItemElement} from 'chrome://os-settings/os_settings.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

suite('<os-settings-menu-item>', () => {
  let menuItem: OsSettingsMenuItemElement;

  async function createMenuItem(label: string, sublabel?: string) {
    menuItem = document.createElement('os-settings-menu-item');
    menuItem.label = label;
    if (sublabel) {
      menuItem.sublabel = sublabel;
    }
    document.body.appendChild(menuItem);
    await flushTasks();
  }

  setup(() => {
    clearBody();
  });

  test('Label is displayed', async () => {
    const expectedLabel = 'Foo';
    await createMenuItem(expectedLabel);

    const actualLabel =
        menuItem.shadowRoot!.querySelector<HTMLElement>('#label')!.innerText;
    assertEquals(expectedLabel, actualLabel);
  });

  test('Sublabel is displayed', async () => {
    const expectedSublabel = 'Bar';
    await createMenuItem('Foo', expectedSublabel);

    const actualSublabel =
        menuItem.shadowRoot!.querySelector<HTMLElement>('#sublabel')!.innerText;
    assertEquals(expectedSublabel, actualSublabel);
  });

  test('Label is reflected to the aria-label attribute', async () => {
    await createMenuItem('Foo');
    assertEquals('Foo', menuItem.getAttribute('aria-label'));
    menuItem.label = 'Bar';
    assertEquals('Bar', menuItem.getAttribute('aria-label'));
  });

  test('Sublabel is reflected to the aria-description attribute', async () => {
    await createMenuItem('Foo', 'Bar');
    assertEquals('Bar', menuItem.getAttribute('aria-description'));
    menuItem.sublabel = 'Baz';
    assertEquals('Baz', menuItem.getAttribute('aria-description'));
  });

  test('Pressing Spacebar key triggers click', async () => {
    await createMenuItem('Foo');
    const clickEventPromise = eventToPromise('click', window);
    menuItem.dispatchEvent(new KeyboardEvent('keydown', {key: ' '}));
    await clickEventPromise;
  });

  test('Pressing Enter key triggers click', async () => {
    await createMenuItem('Foo');
    const clickEventPromise = eventToPromise('click', window);
    menuItem.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    await clickEventPromise;
  });
});

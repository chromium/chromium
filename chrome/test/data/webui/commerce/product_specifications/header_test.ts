// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/header.js';

import type {HeaderElement} from 'chrome://compare/header.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {$$, eventToPromise, hasStyle, isVisible} from 'chrome://webui-test/test_util.js';

suite('HeaderTest', () => {
  let header: HeaderElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    header = document.createElement('product-specifications-header');
    document.body.appendChild(header);
    await flushTasks();
  });

  test('menu shown on click', async () => {
    const menu = header.$.menu.$.menu;

    assertEquals(menu.getIfExists(), null);

    header.$.menuButton.click();
    await flushTasks();

    assertNotEquals(menu.getIfExists(), null);
  });

  test('button changes background color when menu is showing', async () => {
    const menuButton = header.$.menuButton;
    const baseBackgroundColor =
        menuButton.computedStyleMap().get('background-color');
    assertTrue(!!baseBackgroundColor);
    menuButton.click();
    await flushTasks();

    const menuShownBackgroundColor =
        menuButton.computedStyleMap().get('background-color');
    assertTrue(!!menuShownBackgroundColor);
    assertNotEquals(
        menuShownBackgroundColor.toString(), baseBackgroundColor.toString());

    header.$.menu.close();
    menuButton.blur();
    await eventToPromise('close', header.$.menu);

    const menuClosedBackgroundColor =
        menuButton.computedStyleMap().get('background-color');
    assertTrue(!!menuClosedBackgroundColor);
    assertEquals(
        baseBackgroundColor.toString(), menuClosedBackgroundColor.toString());
  });

  test('menu rename shows input', async () => {
    header.$.menuButton.click();
    await flushTasks();

    assertFalse(!!header.shadowRoot!.querySelector('#input'));
    const menu = header.$.menu.$.menu;
    const renameMenuItem = menu.get().querySelector<HTMLElement>('#rename');
    assertTrue(!!renameMenuItem);
    renameMenuItem.click();
    await waitAfterNextRender(header);

    assertTrue(!!header.shadowRoot!.querySelector('#input'));
    assertFalse(menu.get().open);
  });

  test('input hides and changes name on enter', async () => {
    header.$.menu.dispatchEvent(new CustomEvent('rename-click'));
    await waitAfterNextRender(header);

    const input = header.shadowRoot!.querySelector<CrInputElement>('#input');
    assertTrue(!!input);
    assertTrue(isVisible(input));
    const newName = 'new name';
    input.value = newName;
    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    await waitAfterNextRender(input);

    assertFalse(isVisible(input));
    assertEquals(newName, header.subtitle);
  });

  test('setting `subtitle` gives the header a subtitle', async () => {
    const subtitle = $$(header, '#subtitle');
    assertTrue(!!subtitle);
    assertTrue(hasStyle(subtitle, 'display', 'none'));
    assertTrue(hasStyle(header.$.divider, 'display', 'none'));
    assertTrue(hasStyle(header.$.menuButton, 'display', 'none'));

    header.subtitle = 'foo';
    await waitAfterNextRender(header);

    assertEquals('foo', subtitle!.textContent!.trim());
    assertFalse(hasStyle(subtitle, 'display', 'none'));
    assertFalse(hasStyle(header.$.divider, 'display', 'none'));
    assertFalse(hasStyle(header.$.menuButton, 'display', 'none'));
  });

  test('subtitle is editable on enter', async () => {
    const subtitle = $$(header, '#subtitle');
    assertTrue(!!subtitle);
    subtitle.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    await waitAfterNextRender(header);

    const input = header.shadowRoot!.querySelector<CrInputElement>('#input');
    assertTrue(!!input);
    assertTrue(isVisible(input));
  });

  test('subtitle and input do not change after empty input', async () => {
    const subtitle = 'foo';

    header.subtitle = subtitle;
    header.$.menu.dispatchEvent(new CustomEvent('rename-click'));
    await waitAfterNextRender(header);

    const input = header.shadowRoot!.querySelector<CrInputElement>('#input');
    assertTrue(!!input);
    input.value = '';
    input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    await waitAfterNextRender(header);

    // Ensure the cursor is at the end of the input.
    assertTrue(input.$.input.selectionStart === input.value.length);
    assertTrue(input.$.input.selectionEnd === input.value.length);

    assertEquals(subtitle, header.subtitle);
    assertEquals(subtitle, input.value);
  });
});

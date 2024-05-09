// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/header.js';

import type {HeaderElement} from 'chrome://compare/header.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {$$, assertNotStyle, assertStyle} from './test_support.js';

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

  test('setting `subtitle` gives the header a subtitle', async () => {
    assertStyle($$(header, '#subtitle')!, 'display', 'none');
    assertStyle($$(header, '#divider')!, 'display', 'none');
    assertStyle($$(header, '#menuButton')!, 'display', 'none');

    header.subtitle = 'foo';
    await waitAfterNextRender(header);

    assertEquals('foo', $$(header, '#subtitle')!.textContent);
    assertNotStyle($$(header, '#subtitle')!, 'display', 'none');
    assertNotStyle($$(header, '#divider')!, 'display', 'none');
    assertNotStyle($$(header, '#menuButton')!, 'display', 'none');
  });
});

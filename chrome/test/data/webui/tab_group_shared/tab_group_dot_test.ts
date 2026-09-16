// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tab-search.top-chrome/strings.m.js';
import 'chrome://tab-search.top-chrome/tab_group_shared/tab_group_dot.js';

import type {TabGroupDotElement} from 'chrome://tab-search.top-chrome/tab_group_shared/tab_group_dot.js';
import {Color} from 'chrome://tab-search.top-chrome/tab_group_shared/tab_group_types.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('TabGroupDotTest', () => {
  let dot: TabGroupDotElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dot = document.createElement('tab-group-dot');
    document.body.appendChild(dot);
    await microtasksFinished();
  });

  test('renders svg and circle', () => {
    const svg = dot.shadowRoot.querySelector('svg');
    assertTrue(!!svg);
    assertEquals('-5 -5 10 10', svg.getAttribute('viewBox'));

    const circle = dot.shadowRoot.querySelector('circle');
    assertTrue(!!circle);
    assertEquals('0', circle.getAttribute('cx'));
    assertEquals('0', circle.getAttribute('cy'));
    assertEquals('4', circle.getAttribute('r'));
  });

  test('sets color without refresh', async () => {
    dot.tabGroupColorRefresh_ = false;
    dot.color = Color.kBlue;
    await microtasksFinished();

    assertEquals(
        'var(--tab-group-color-blue)',
        dot.style.getPropertyValue('--group-dot-color'));

    dot.color = Color.kRed;
    await microtasksFinished();

    assertEquals(
        'var(--tab-group-color-red)',
        dot.style.getPropertyValue('--group-dot-color'));
  });

  test('sets color with refresh', async () => {
    dot.tabGroupColorRefresh_ = true;
    dot.color = Color.kBlue;
    await microtasksFinished();

    assertEquals(
        'var(--tab-group-refresh-color-blue)',
        dot.style.getPropertyValue('--group-dot-color'));

    dot.color = Color.kGreen;
    await microtasksFinished();

    assertEquals(
        'var(--tab-group-refresh-color-green)',
        dot.style.getPropertyValue('--group-dot-color'));
  });
});

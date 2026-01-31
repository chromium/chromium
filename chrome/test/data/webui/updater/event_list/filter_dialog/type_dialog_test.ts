// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilterCategory} from 'chrome://updater/event_list/filter_bar.js';
import {TypeDialogElement} from 'chrome://updater/event_list/filter_dialog/type_dialog.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('TypeDialogElement', () => {
  let filterType: TypeDialogElement;

  setup(() => {
    filterType = new TypeDialogElement();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(filterType);
  });

  test('renders correctly', () => {
    assertTrue(filterType instanceof HTMLElement);
    assertEquals('TYPE-DIALOG', filterType.tagName);
  });

  test('displays menu items', async () => {
    await microtasksFinished();
    const items = filterType.shadowRoot.querySelectorAll('.filter-menu-item');
    assertEquals(5, items.length);
    assertEquals('App', items[0]!.textContent.trim());
    assertEquals('Event Type', items[1]!.textContent.trim());
    assertEquals('Update Outcome', items[2]!.textContent.trim());
    assertEquals('Updater Scope', items[3]!.textContent.trim());
    assertEquals('Date', items[4]!.textContent.trim());
  });

  test('fires type-selection-changed event on click', async () => {
    await microtasksFinished();
    let capturedEvent: CustomEvent<FilterCategory>|null = null;
    filterType.addEventListener('type-selection-changed', (e: Event) => {
      capturedEvent = e as CustomEvent<FilterCategory>;
    });

    const items = filterType.shadowRoot.querySelectorAll<HTMLElement>(
        '.filter-menu-item');
    items[0]!.click();
    await microtasksFinished();

    assertNotEquals(null, capturedEvent);
    assertEquals(FilterCategory.APP, capturedEvent!.detail);
  });

  test('focuses first menu item on load', async () => {
    await microtasksFinished();
    const item =
        filterType.shadowRoot.querySelector<HTMLElement>('.filter-menu-item');
    assertEquals(filterType.shadowRoot.activeElement, item);
  });
});

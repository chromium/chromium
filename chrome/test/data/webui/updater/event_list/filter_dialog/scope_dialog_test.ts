// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrCheckboxElement} from '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {Scope} from 'chrome://updater/event_history.js';
import {ScopeDialogElement} from 'chrome://updater/event_list/filter_dialog/scope_dialog.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('ScopeDialogElement', () => {
  let filterScope: ScopeDialogElement;

  setup(() => {
    filterScope = new ScopeDialogElement();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(filterScope);
  });

  test('renders correctly', () => {
    assertTrue(filterScope instanceof HTMLElement);
    assertEquals('SCOPE-DIALOG', filterScope.tagName);
  });

  test('displays scopes', async () => {
    await microtasksFinished();
    const checkboxes = filterScope.shadowRoot.querySelectorAll('cr-checkbox');
    assertEquals(2, checkboxes.length);
  });

  test('initializes with selections', async () => {
    filterScope.initialSelections = new Set(['USER']);
    await microtasksFinished();

    const userCheckbox =
        filterScope.shadowRoot.querySelector<CrCheckboxElement>(
            'cr-checkbox[data-scope="USER"]');
    assertTrue(!!userCheckbox);
    const systemCheckbox =
        filterScope.shadowRoot.querySelector<CrCheckboxElement>(
            'cr-checkbox[data-scope="SYSTEM"]');
    assertTrue(!!systemCheckbox);

    assertTrue(userCheckbox.checked);
    assertFalse(systemCheckbox.checked);
  });

  test('fires filter-change event on apply', async () => {
    await microtasksFinished();
    const userCheckbox = filterScope.shadowRoot.querySelector<HTMLElement>(
        'cr-checkbox[data-scope="USER"]');
    userCheckbox!.click();
    await microtasksFinished();

    const eventFired = new Promise<CustomEvent<Set<Scope>>>(resolve => {
      filterScope.addEventListener('filter-change', (e: Event) => {
        resolve(e as CustomEvent<Set<Scope>>);
      }, {once: true});
    });
    const footerElement =
        filterScope.shadowRoot?.querySelector('filter-dialog-footer');
    assertTrue(!!footerElement);
    const applyButton =
        footerElement.shadowRoot.querySelector<HTMLElement>('.action-button')!;
    applyButton.click();
    const capturedEvent = await eventFired;

    assertTrue(!!capturedEvent);
    assertTrue(capturedEvent.detail.has('USER'));
  });

  test('focuses first checkbox on load', async () => {
    await microtasksFinished();
    const checkbox =
        filterScope.shadowRoot.querySelector<HTMLElement>('.filter-menu-item');
    assertEquals(checkbox, filterScope.shadowRoot.activeElement);
  });
});

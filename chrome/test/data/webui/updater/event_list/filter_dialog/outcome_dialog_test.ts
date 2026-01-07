// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrCheckboxElement} from '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {OutcomeDialogElement} from 'chrome://updater/event_list/filter_dialog/outcome_dialog.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('OutcomeDialogElement', () => {
  let filterOutcome: OutcomeDialogElement;

  setup(() => {
    filterOutcome = new OutcomeDialogElement();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(filterOutcome);
  });

  test('renders correctly', () => {
    assertTrue(filterOutcome instanceof HTMLElement);
    assertEquals('OUTCOME-DIALOG', filterOutcome.tagName);
  });

  test('displays update outcomes', async () => {
    await microtasksFinished();
    const checkboxes = filterOutcome.shadowRoot.querySelectorAll('cr-checkbox');
    // UPDATED, NO_UPDATE, UPDATE_ERROR
    assertEquals(3, checkboxes.length);
  });

  test('initializes with selections', async () => {
    filterOutcome.initialSelections = new Set(['UPDATED']);
    await microtasksFinished();

    const updatedCheckbox =
        filterOutcome.shadowRoot.querySelector<CrCheckboxElement>(
            'cr-checkbox[data-outcome="UPDATED"]');
    assertNotEquals(null, updatedCheckbox);
    assertTrue(updatedCheckbox!.checked);
  });

  test('fires filter-change event on apply', async () => {
    await microtasksFinished();
    const updatedCheckbox = filterOutcome.shadowRoot.querySelector<HTMLElement>(
        'cr-checkbox[data-outcome="UPDATED"]');
    updatedCheckbox!.click();
    await microtasksFinished();

    let capturedEvent: CustomEvent<Set<string>>|null = null;
    filterOutcome.addEventListener('filter-change', (e: Event) => {
      capturedEvent = e as CustomEvent<Set<string>>;
    });

    const footerElement =
        filterOutcome.shadowRoot?.querySelector('filter-dialog-footer');
    assertNotEquals(null, footerElement);
    const applyButton = footerElement!.shadowRoot?.querySelector<HTMLElement>(
        '.action-button')!;
    applyButton.click();
    await microtasksFinished();

    assertNotEquals(null, capturedEvent);
    assertTrue(capturedEvent!.detail.has('UPDATED'));
  });

  test('focuses first checkbox on load', async () => {
    await microtasksFinished();
    const checkbox = filterOutcome.shadowRoot.querySelector<HTMLElement>(
        '.filter-menu-item');
    assertEquals(checkbox, filterOutcome.shadowRoot.activeElement);
  });
});

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DateDialogElement} from 'chrome://updater/event_list/filter_dialog/date_dialog.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('DateDialogElement', () => {
  let filterDate: DateDialogElement;

  setup(() => {
    filterDate = new DateDialogElement();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(filterDate);
  });

  test('renders correctly', () => {
    assertTrue(filterDate instanceof HTMLElement);
    assertEquals('DATE-DIALOG', filterDate.tagName);
  });

  test('displays date inputs', async () => {
    await microtasksFinished();
    const inputs =
        filterDate.shadowRoot.querySelectorAll('input[type="datetime-local"]');
    assertEquals(2, inputs.length);
  });

  test('initializes with dates', async () => {
    const start = new Date('2025-01-01T10:00');
    const end = new Date('2025-01-02T10:00');
    filterDate.initialStartDate = start;
    filterDate.initialEndDate = end;
    await microtasksFinished();

    const startInput =
        filterDate.shadowRoot.querySelector<HTMLInputElement>('#start-date')!;
    const endInput =
        filterDate.shadowRoot.querySelector<HTMLInputElement>('#end-date')!;

    // valueAsNumber returns milliseconds
    assertEquals(start.getTime(), startInput.valueAsNumber);
    assertEquals(end.getTime(), endInput.valueAsNumber);
  });

  test('fires filter-change event on apply', async () => {
    await microtasksFinished();
    const startInput =
        filterDate.shadowRoot.querySelector<HTMLInputElement>('#start-date')!;
    startInput.valueAsNumber = new Date('2025-01-01T00:00').getTime();
    startInput.dispatchEvent(new Event('input'));
    await microtasksFinished();

    let capturedEvent: CustomEvent<{start: Date | null, end: Date | null}>|
        null = null;
    filterDate.addEventListener('filter-change', (e: Event) => {
      capturedEvent = e as CustomEvent<{start: Date | null, end: Date | null}>;
    });

    const footerElement =
        filterDate.shadowRoot?.querySelector('filter-dialog-footer');
    assertNotEquals(null, footerElement);
    const applyButton = footerElement!.shadowRoot?.querySelector<HTMLElement>(
        '.action-button')!;
    applyButton.click();
    await microtasksFinished();

    assertNotEquals(null, capturedEvent);
    assertNotEquals(null, capturedEvent!.detail.start);
    assertEquals(
        new Date('2025-01-01T00:00').getTime(),
        capturedEvent!.detail.start!.getTime());
  });

  test('focuses start date input on load', async () => {
    await microtasksFinished();
    const input = filterDate.shadowRoot.querySelector<HTMLElement>(
        '.filter-menu-date-inputs input');
    assertEquals(filterDate.shadowRoot.activeElement, input);
  });
});

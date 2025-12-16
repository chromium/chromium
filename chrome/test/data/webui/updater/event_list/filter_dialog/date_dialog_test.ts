// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {expect} from '//webui-test/chai.js';
import {DateDialogElement} from 'chrome://updater/event_list/filter_dialog/date_dialog.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('DateDialogElement', () => {
  let filterDate: DateDialogElement;

  setup(() => {
    filterDate = new DateDialogElement();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(filterDate);
  });

  test('renders correctly', () => {
    expect(filterDate instanceof HTMLElement).to.be.true;
    expect(filterDate.tagName).to.equal('DATE-DIALOG');
  });

  test('displays date inputs', async () => {
    await microtasksFinished();
    const inputs =
        filterDate.shadowRoot.querySelectorAll('input[type="datetime-local"]');
    expect(inputs.length).to.equal(2);
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
    expect(startInput.valueAsNumber).to.equal(start.getTime());
    expect(endInput.valueAsNumber).to.equal(end.getTime());
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
    expect(footerElement).to.not.be.null;
    const applyButton = footerElement!.shadowRoot?.querySelector<HTMLElement>(
        '.action-button')!;
    applyButton.click();
    await microtasksFinished();

    expect(capturedEvent).to.not.be.null;
    expect(capturedEvent!.detail.start).to.not.be.null;
    expect(capturedEvent!.detail.start!.getTime())
        .to.equal(new Date('2025-01-01T00:00').getTime());
  });
});

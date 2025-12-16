// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrCheckboxElement} from '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {expect} from '//webui-test/chai.js';
import {OutcomeDialogElement} from 'chrome://updater/event_list/filter_dialog/outcome_dialog.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('OutcomeDialogElement', () => {
  let filterOutcome: OutcomeDialogElement;

  setup(() => {
    filterOutcome = new OutcomeDialogElement();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(filterOutcome);
  });

  test('renders correctly', () => {
    expect(filterOutcome instanceof HTMLElement).to.be.true;
    expect(filterOutcome.tagName).to.equal('OUTCOME-DIALOG');
  });

  test('displays update outcomes', async () => {
    await microtasksFinished();
    const checkboxes = filterOutcome.shadowRoot.querySelectorAll('cr-checkbox');
    // UPDATED, NO_UPDATE, UPDATE_ERROR
    expect(checkboxes.length).to.equal(3);
  });

  test('initializes with selections', async () => {
    filterOutcome.initialSelections = new Set(['UPDATED']);
    await microtasksFinished();

    const updatedCheckbox =
        filterOutcome.shadowRoot.querySelector<CrCheckboxElement>(
            'cr-checkbox[data-outcome="UPDATED"]');
    expect(updatedCheckbox).to.not.be.null;
    expect(updatedCheckbox!.checked).to.be.true;
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
    expect(footerElement).to.not.be.null;
    const applyButton = footerElement!.shadowRoot?.querySelector<HTMLElement>(
        '.action-button')!;
    applyButton.click();
    await microtasksFinished();

    expect(capturedEvent).to.not.be.null;
    expect(capturedEvent!.detail.has('UPDATED')).to.be.true;
  });
});

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrCheckboxElement} from '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {expect} from '//webui-test/chai.js';
import {EventDialogElement} from 'chrome://updater/event_list/filter_dialog/event_dialog.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('EventDialogElement', () => {
  let filterEvent: EventDialogElement;

  setup(() => {
    filterEvent = new EventDialogElement();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(filterEvent);
  });

  test('renders correctly', () => {
    expect(filterEvent instanceof HTMLElement).to.be.true;
    expect(filterEvent.tagName).to.equal('EVENT-DIALOG');
  });

  test('displays event types', async () => {
    await microtasksFinished();
    const checkboxes = filterEvent.shadowRoot.querySelectorAll('cr-checkbox');
    // We expect at least the common ones: Update, Install, Uninstall
    expect(checkboxes.length).to.be.at.least(3);

    // Check for common events headers
    const headers =
        filterEvent.shadowRoot.querySelectorAll('.filter-menu-section-header');
    expect(headers.length).to.equal(2);
    expect(headers[0]!.textContent).to.equal('Common');
    expect(headers[1]!.textContent).to.equal('Other');
  });

  test('initializes with selections', async () => {
    filterEvent.initialSelections = new Set(['INSTALL']);
    await microtasksFinished();

    const installCheckbox =
        filterEvent.shadowRoot.querySelector<CrCheckboxElement>(
            'cr-checkbox[data-event-type="INSTALL"]');
    expect(installCheckbox).to.not.be.null;
    expect(installCheckbox!.checked).to.be.true;
  });

  test('fires filter-change event on apply', async () => {
    await microtasksFinished();
    const updateCheckbox = filterEvent.shadowRoot.querySelector<HTMLElement>(
        'cr-checkbox[data-event-type="UPDATE"]');
    updateCheckbox!.click();
    await microtasksFinished();

    let capturedEvent: CustomEvent<Set<string>>|null = null;
    filterEvent.addEventListener('filter-change', (e: Event) => {
      capturedEvent = e as CustomEvent<Set<string>>;
    });

    const footerElement =
        filterEvent.shadowRoot?.querySelector('filter-dialog-footer');
    expect(footerElement).to.not.be.null;
    const applyButton = footerElement!.shadowRoot?.querySelector<HTMLElement>(
        '.action-button')!;
    applyButton.click();
    await microtasksFinished();

    expect(capturedEvent).to.not.be.null;
    expect(capturedEvent!.detail.has('UPDATE')).to.be.true;
  });

  test('fires close event on cancel', async () => {
    let closeFired = false;
    filterEvent.addEventListener('close', () => {
      closeFired = true;
    });

    const footerElement =
        filterEvent.shadowRoot?.querySelector('filter-dialog-footer');
    expect(footerElement).to.not.be.null;
    const cancelButton = footerElement!.shadowRoot?.querySelector<HTMLElement>(
        '.cancel-button')!;
    cancelButton.click();
    await microtasksFinished();

    expect(closeFired).to.be.true;
  });
});

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrCheckboxElement} from '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {EventDialogElement} from 'chrome://updater/event_list/filter_dialog/event_dialog.js';
import {assertEquals, assertGE, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('EventDialogElement', () => {
  let filterEvent: EventDialogElement;

  setup(() => {
    filterEvent = new EventDialogElement();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(filterEvent);
  });

  test('renders correctly', () => {
    assertTrue(filterEvent instanceof HTMLElement);
    assertEquals('EVENT-DIALOG', filterEvent.tagName);
  });

  test('displays event types', async () => {
    await microtasksFinished();
    const checkboxes = filterEvent.shadowRoot.querySelectorAll('cr-checkbox');
    // We expect at least the common ones: Update, Install, Uninstall
    assertGE(checkboxes.length, 3);

    // Check for common events headers
    const headers =
        filterEvent.shadowRoot.querySelectorAll('.filter-menu-section-header');
    assertEquals(2, headers.length);
    assertEquals('Common', headers[0]!.textContent);
    assertEquals('Other', headers[1]!.textContent);
  });

  test('initializes with selections', async () => {
    filterEvent.initialSelections = new Set(['INSTALL']);
    await microtasksFinished();

    const installCheckbox =
        filterEvent.shadowRoot.querySelector<CrCheckboxElement>(
            'cr-checkbox[data-event-type="INSTALL"]');
    assertNotEquals(null, installCheckbox);
    assertTrue(installCheckbox!.checked);
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
    assertNotEquals(null, footerElement);
    const applyButton = footerElement!.shadowRoot?.querySelector<HTMLElement>(
        '.action-button')!;
    applyButton.click();
    await microtasksFinished();

    assertNotEquals(null, capturedEvent);
    assertTrue(capturedEvent!.detail.has('UPDATE'));
  });

  test('fires close event on cancel', async () => {
    let closeFired = false;
    filterEvent.addEventListener('close', () => {
      closeFired = true;
    });

    const footerElement =
        filterEvent.shadowRoot?.querySelector('filter-dialog-footer');
    assertNotEquals(null, footerElement);
    const cancelButton = footerElement!.shadowRoot?.querySelector<HTMLElement>(
        '.cancel-button')!;
    cancelButton.click();
    await microtasksFinished();

    assertTrue(closeFired);
  });

  test('focuses first checkbox on load', async () => {
    await microtasksFinished();
    const checkbox =
        filterEvent.shadowRoot.querySelector<HTMLElement>('.filter-menu-item');
    assertEquals(filterEvent.shadowRoot.activeElement, checkbox);
  });
});

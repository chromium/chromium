// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {AppDialogElement} from 'chrome://updater/event_list/filter_dialog/app_dialog.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('AppDialogElement', () => {
  let filterApp: AppDialogElement;

  setup(() => {
    loadTimeData.overrideValues({
      'numKnownApps': 2,
      'knownAppName0': 'Google Chrome',
      'knownAppIds0': 'COM.GOOGLE.CHROME"',
      'knownAppName1': 'Google Chrome Beta',
      'knownAppIds1': 'COM.GOOGLE.CHROME.BETA"',
    });

    filterApp = new AppDialogElement();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(filterApp);
  });

  test('renders correctly', () => {
    assertTrue(filterApp instanceof HTMLElement);
    assertEquals('APP-DIALOG', filterApp.tagName);
  });

  test('displays known apps', async () => {
    await microtasksFinished();
    const checkboxes = filterApp.shadowRoot.querySelectorAll('cr-checkbox');
    assertEquals(2, checkboxes.length);
    assertEquals('Google Chrome', checkboxes[0]!.textContent.trim());
    assertEquals('Google Chrome Beta', checkboxes[1]!.textContent.trim());
  });

  test('filters apps by search', async () => {
    await microtasksFinished();
    const input = filterApp.shadowRoot.querySelector('input')!;
    input.value = 'Beta';
    input.dispatchEvent(new InputEvent('input'));
    await microtasksFinished();

    const checkboxes = filterApp.shadowRoot.querySelectorAll('cr-checkbox');
    assertEquals(2, checkboxes.length);
    assertEquals('Google Chrome Beta', checkboxes[0]!.textContent.trim());
    assertEquals('Beta', checkboxes[1]!.textContent.trim());
  });

  test('initializes with selections', async () => {
    filterApp.initialSelections = new Set(['Google Chrome']);
    await microtasksFinished();

    const checkboxes = filterApp.shadowRoot.querySelectorAll('cr-checkbox');
    assertTrue(checkboxes[0]!.checked);
    assertFalse(checkboxes[1]!.checked);
  });

  test('fires filter-change event on apply', async () => {
    await microtasksFinished();
    const checkboxes = filterApp.shadowRoot.querySelectorAll('cr-checkbox');
    checkboxes[0]!.click();
    await microtasksFinished();

    let capturedEvent: CustomEvent<Set<string>>|null = null;
    filterApp.addEventListener('filter-change', (e: Event) => {
      capturedEvent = e as CustomEvent<Set<string>>;
    });

    const footerElement =
        filterApp.shadowRoot?.querySelector('filter-dialog-footer');
    assertNotEquals(null, footerElement);
    const applyButton = footerElement!.shadowRoot?.querySelector<HTMLElement>(
        '.action-button')!;
    applyButton.click();
    await microtasksFinished();

    assertNotEquals(null, capturedEvent);
    assertTrue(capturedEvent!.detail.has('Google Chrome'));
    assertEquals(1, capturedEvent!.detail.size);
  });

  test('fires close event on cancel', async () => {
    let closeFired = false;
    filterApp.addEventListener('close', () => {
      closeFired = true;
    });

    const footerElement =
        filterApp.shadowRoot?.querySelector('filter-dialog-footer');
    assertNotEquals(null, footerElement);
    const cancelButton = footerElement!.shadowRoot?.querySelector<HTMLElement>(
        '.cancel-button')!;
    cancelButton.click();
    await microtasksFinished();

    assertTrue(closeFired);
  });

  test('focuses input on load', async () => {
    await microtasksFinished();
    const input =
        filterApp.shadowRoot.querySelector<HTMLElement>('.filter-menu-input');
    assertEquals(filterApp.shadowRoot.activeElement, input);
  });
});

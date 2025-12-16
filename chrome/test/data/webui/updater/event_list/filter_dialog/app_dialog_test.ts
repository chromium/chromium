// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {expect} from '//webui-test/chai.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {AppDialogElement} from 'chrome://updater/event_list/filter_dialog/app_dialog.js';
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
    expect(filterApp instanceof HTMLElement).to.be.true;
    expect(filterApp.tagName).to.equal('APP-DIALOG');
  });

  test('displays known apps', async () => {
    await microtasksFinished();
    const checkboxes = filterApp.shadowRoot.querySelectorAll('cr-checkbox');
    expect(checkboxes.length).to.equal(2);
    expect(checkboxes[0]!.textContent.trim()).to.equal('Google Chrome');
    expect(checkboxes[1]!.textContent.trim()).to.equal('Google Chrome Beta');
  });

  test('filters apps by search', async () => {
    await microtasksFinished();
    const input = filterApp.shadowRoot.querySelector('input')!;
    input.value = 'Beta';
    input.dispatchEvent(new InputEvent('input'));
    await microtasksFinished();

    const checkboxes = filterApp.shadowRoot.querySelectorAll('cr-checkbox');
    expect(checkboxes.length).to.equal(2);
    expect(checkboxes[0]!.textContent.trim()).to.equal('Google Chrome Beta');
    expect(checkboxes[1]!.textContent.trim()).to.equal('Beta');
  });

  test('initializes with selections', async () => {
    filterApp.initialSelections = new Set(['Google Chrome']);
    await microtasksFinished();

    const checkboxes = filterApp.shadowRoot.querySelectorAll('cr-checkbox');
    expect(checkboxes[0]!.checked).to.be.true;
    expect(checkboxes[1]!.checked).to.be.false;
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
    expect(footerElement).to.not.be.null;
    const applyButton = footerElement!.shadowRoot?.querySelector<HTMLElement>(
        '.action-button')!;
    applyButton.click();
    await microtasksFinished();

    expect(capturedEvent).to.not.be.null;
    expect(capturedEvent!.detail.has('Google Chrome')).to.be.true;
    expect(capturedEvent!.detail.size).to.equal(1);
  });

  test('fires close event on cancel', async () => {
    let closeFired = false;
    filterApp.addEventListener('close', () => {
      closeFired = true;
    });

    const footerElement =
        filterApp.shadowRoot?.querySelector('filter-dialog-footer');
    expect(footerElement).to.not.be.null;
    const cancelButton = footerElement!.shadowRoot?.querySelector<HTMLElement>(
        '.cancel-button')!;
    cancelButton.click();
    await microtasksFinished();

    expect(closeFired).to.be.true;
  });
});

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://omnibox-everywhere.top-chrome/fre_modal.js';

import type {FreModalElement} from 'chrome://omnibox-everywhere.top-chrome/fre_modal.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

suite('FreModalTest', () => {
  let freModal: FreModalElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.resetForTesting({
      loomniboxFreTitle: 'Ask Google from anywhere',
      loomniboxFreLensPrimary:
          'Use the lens button to share what\'s on your screen',
      loomniboxFreLensSecondary:
          'Add context to your question by selecting an area or ' +
          'your entire screen',
      loomniboxFreKeyboardPrimary: 'Easily open with',
      loomniboxFreKeyboardBadgeOption: 'Option',
      loomniboxFreKeyboardBadgeSpace: 'Space',
      loomniboxFreAcceptHotkey: 'Accept hotkey',
      loomniboxFreOr: 'or',
      loomniboxFreEditOwn: 'edit your own',
      loomniboxFreCloseButtonAria: 'Close',
      loomniboxFreEditButtonAria: 'Edit Hotkey',
    });
    freModal = document.createElement('fre-modal');
    document.body.appendChild(freModal);
    await freModal.updateComplete;
  });

  test('renders all content correctly', () => {
    assertTrue(isVisible(freModal));
    const title = freModal.shadowRoot.querySelector('.title');
    assertTrue(!!title);

    const closeBtn =
        freModal.shadowRoot.querySelector<HTMLElement>('.close-button');
    assertTrue(!!closeBtn);

    const keyBadges =
        freModal.shadowRoot.querySelectorAll<HTMLElement>('.key-badge');
    assertEquals(2, keyBadges.length);

    const acceptBtn = freModal.shadowRoot.querySelector<HTMLButtonElement>(
        '.accept-hotkey-btn');
    assertTrue(!!acceptBtn);

    const editLink =
        freModal.shadowRoot.querySelector<HTMLAnchorElement>('.edit-link');
    assertTrue(!!editLink);
  });

  test('clicking close button fires close event', async () => {
    const closeBtn =
        freModal.shadowRoot.querySelector<HTMLElement>('.close-button')!;
    const closePromise = eventToPromise('close', freModal);
    closeBtn.click();
    await closePromise;
  });

  test('clicking accept hotkey button fires accept-hotkey event', async () => {
    const acceptBtn = freModal.shadowRoot.querySelector<HTMLButtonElement>(
        '.accept-hotkey-btn')!;
    const acceptPromise = eventToPromise('accept-hotkey', freModal);
    acceptBtn.click();
    await acceptPromise;
  });

  test('clicking edit-link fires open-settings event', async () => {
    const editLink =
        freModal.shadowRoot.querySelector<HTMLAnchorElement>('.edit-link')!;
    const settingsPromise = eventToPromise('open-settings', freModal);
    editLink.click();
    await settingsPromise;
  });
});

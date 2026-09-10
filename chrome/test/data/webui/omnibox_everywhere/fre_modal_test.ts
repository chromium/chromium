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
      loomniboxFreTitle: 'Search with Chrome from anywhere',
      loomniboxFreLensPrimary: 'Share what’s on your screen with Google Lens',
      loomniboxFreLensSecondary:
          'Ask about content outside of Chrome, like another app or file ' +
          'you have open',
      loomniboxFreWhereToFindPrimary: 'Open from the Mac menu bar',
      loomniboxFreCloseButtonAria: 'Close',
      isFuseboxEligible: true,
    });
    freModal = document.createElement('fre-modal');
    document.body.appendChild(freModal);
    await freModal.updateComplete;
  });

  test('renders all content correctly', () => {
    assertTrue(isVisible(freModal));
    const title = freModal.shadowRoot.querySelector('.title');
    assertTrue(!!title);
    assertEquals('Search with Chrome from anywhere', title.textContent.trim());

    const logo = freModal.shadowRoot.querySelector('.chrome-logo');
    assertTrue(!!logo);

    const closeBtn =
        freModal.shadowRoot.querySelector<HTMLElement>('.close-button');
    assertTrue(!!closeBtn);

    const listItems =
        freModal.shadowRoot.querySelectorAll<HTMLElement>('.list-item');
    assertEquals(2, listItems.length);

    const lensPrimary = listItems[0]!.querySelector('.primary-text');
    assertTrue(!!lensPrimary);
    assertEquals(
        'Share what’s on your screen with Google Lens',
        lensPrimary.textContent.trim());

    const whereToFindPrimary = listItems[1]!.querySelector('.primary-text');
    assertTrue(!!whereToFindPrimary);
    assertEquals(
        'Open from the Mac menu bar', whereToFindPrimary.textContent.trim());

    const openInNewIcon = listItems[1]!.querySelector('.open-in-new-icon');
    assertTrue(!!openInNewIcon);

    const img = listItems[1]!.querySelector('img');
    assertTrue(!!img);
  });

  test('clicking close button fires close event', async () => {
    const closeBtn =
        freModal.shadowRoot.querySelector<HTMLElement>('.close-button')!;
    const closePromise = eventToPromise('close', freModal);
    closeBtn.click();
    await closePromise;
  });

  test('omits lens value prop row when not fusebox eligible', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.resetForTesting({
      loomniboxFreTitle: 'Search with Chrome from anywhere',
      loomniboxFreLensPrimary: 'Share what’s on your screen with Google Lens',
      loomniboxFreLensSecondary:
          'Ask about content outside of Chrome, like another app or file ' +
          'you have open',
      loomniboxFreWhereToFindPrimary: 'Open from the Mac menu bar',
      loomniboxFreCloseButtonAria: 'Close',
      isFuseboxEligible: false,
    });
    freModal = document.createElement('fre-modal');
    document.body.appendChild(freModal);
    await freModal.updateComplete;

    const listItems =
        freModal.shadowRoot.querySelectorAll<HTMLElement>('.list-item');
    assertEquals(1, listItems.length);

    const whereToFindPrimary = listItems[0]!.querySelector('.primary-text');
    assertTrue(!!whereToFindPrimary);
    assertEquals(
        'Open from the Mac menu bar', whereToFindPrimary.textContent.trim());
  });
});

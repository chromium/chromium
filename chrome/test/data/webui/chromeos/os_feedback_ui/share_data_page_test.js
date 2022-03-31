// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ShareDataPageElement} from 'chrome://os-feedback/share_data_page.js';
import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function shareDataPageTestSuite() {
  /** @type {?ShareDataPageElement} */
  let page = null;

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    page.remove();
    page = null;
  });

  function initializePage() {
    assertFalse(!!page);
    page =
        /** @type {!ShareDataPageElement} */ (
            document.createElement('share-data-page'));
    assertTrue(!!page);
    document.body.appendChild(page);
    return flushTasks();
  }

  // Test the page is loaded with expected HTML elements.
  test('shareDataPageLoaded', async () => {
    await initializePage();
    // Verify the title is in the page.
    const title = page.shadowRoot.querySelector('#title');
    assertTrue(!!title);
    assertEquals('Send feedback', title.textContent);

    // Verify the back button is in the page.
    const buttonBack = page.shadowRoot.querySelector('#buttonBack');
    assertTrue(!!buttonBack);
    assertEquals('Back', buttonBack.textContent.trim());

    // Verify the send button is in the page.
    const buttonSend = page.shadowRoot.querySelector('#buttonSend');
    assertTrue(!!buttonSend);
    assertEquals('Send', buttonSend.textContent.trim());
  });
}

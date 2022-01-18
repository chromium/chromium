// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SearchPageElement} from 'chrome://os-feedback/search_page.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function searchPageTestSuite() {
  /** @type {?SearchPageElement} */
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
        /** @type {!SearchPageElement} */ (
            document.createElement('search-page'));
    assertTrue(!!page);
    document.body.appendChild(page);
    return flushTasks();
  }

  test('SearchPageLoaded', () => {
    return initializePage().then(() => {
      // Verify the title is in the page
      const title = page.shadowRoot.querySelector('#title');
      assertTrue(!!title);
      assertEquals('Send feedback', title.textContent);

      // Verify the continue button is in the page
      const btnContinue = page.shadowRoot.querySelector('#btnContinue');
      assertTrue(!!btnContinue);
    });
  });
}
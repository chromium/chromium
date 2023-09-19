// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_mojom_imports.js';
import 'chrome://scanning/action_toolbar.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('actionToolbarTest', function() {
  /** @type {?ActionToolbarElement} */
  let actionToolbar = null;

  setup(() => {
    actionToolbar = /** @type {!ActionToolbarElement} */ (
        document.createElement('action-toolbar'));
    assertTrue(!!actionToolbar);
    document.body.appendChild(actionToolbar);
  });

  teardown(() => {
    if (actionToolbar) {
      actionToolbar.remove();
    }
    actionToolbar = null;
  });

  // Verify the page count text updates when the number of scanned images
  // changes.
  test('totalPageCountIncrements', () => {
    actionToolbar.pageIndex = 0;
    assertEquals(
        '',
        actionToolbar.shadowRoot.querySelector('#pageNumbers')
            .textContent.trim());

    actionToolbar.numTotalPages = 3;
    assertEquals(
        '1 of 3',
        actionToolbar.shadowRoot.querySelector('#pageNumbers')
            .textContent.trim());

    actionToolbar.numTotalPages = 4;
    actionToolbar.pageIndex = 1;
    assertEquals(
        '2 of 4',
        actionToolbar.shadowRoot.querySelector('#pageNumbers')
            .textContent.trim());
  });

  // Verify clicking the remove page button fires the 'show-remove-page-dialog'
  // event with the correct page number.
  test('removePageClick', () => {
    const expectedPageIndex = 5;
    let pageIndexFromEvent = -1;

    actionToolbar.pageIndex = expectedPageIndex;
    actionToolbar.addEventListener('show-remove-page-dialog', (e) => {
      pageIndexFromEvent = e.detail;
    });

    actionToolbar.shadowRoot.querySelector('#removePageIcon').click();
    return flushTasks().then(() => {
      assertEquals(expectedPageIndex, pageIndexFromEvent);
    });
  });

  // Verify clicking the rescan page button fires the 'show-rescan-page-dialog'
  // event with the correct page number.
  test('rescanPageClick', () => {
    const expectedPageIndex = 5;
    let pageIndexFromEvent = -1;

    actionToolbar.pageIndex = expectedPageIndex;
    actionToolbar.addEventListener('show-rescan-page-dialog', (e) => {
      pageIndexFromEvent = e.detail;
    });

    actionToolbar.shadowRoot.querySelector('#rescanPageIcon').click();
    return flushTasks().then(() => {
      assertEquals(expectedPageIndex, pageIndexFromEvent);
    });
  });
});

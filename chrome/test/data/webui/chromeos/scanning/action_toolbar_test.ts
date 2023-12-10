// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';
import 'chrome://scanning/action_toolbar.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {ActionToolbarElement} from 'chrome://scanning/action_toolbar.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/chromeos/test_util.js';

suite('actionToolbarTest', function() {
  let actionToolbar: ActionToolbarElement|null = null;

  setup(() => {
    actionToolbar = document.createElement('action-toolbar');
    assertTrue(!!actionToolbar);
    document.body.appendChild(actionToolbar);
  });

  teardown(() => {
    actionToolbar?.remove();
    actionToolbar = null;
  });

  function getPageNumberText(): string {
    assert(actionToolbar);
    const pageNumbers =
        strictQuery('#pageNumbers', actionToolbar.shadowRoot, HTMLElement);
    assertTrue(!!pageNumbers);
    return pageNumbers.textContent!.trim();
  }

  // Verify the page count text updates when the number of scanned images
  // changes.
  test('totalPageCountIncrements', () => {
    assert(actionToolbar);
    actionToolbar.pageIndex = 0;
    assertEquals('', getPageNumberText());

    actionToolbar.numTotalPages = 3;
    assertEquals('1 of 3', getPageNumberText());

    actionToolbar.numTotalPages = 4;
    actionToolbar.pageIndex = 1;
    assertEquals('2 of 4', getPageNumberText());
  });

  // Verify clicking the remove page button fires the 'show-remove-page-dialog'
  // event with the correct page number.
  test('removePageClick', async () => {
    assert(actionToolbar);
    const expectedPageIndex = 5;
    let pageIndexFromEvent = -1;

    actionToolbar.pageIndex = expectedPageIndex;
    const removePageEvent: Promise<CustomEvent<number>> =
        eventToPromise('show-remove-page-dialog', actionToolbar);
    strictQuery('#removePageIcon', actionToolbar.shadowRoot!, HTMLElement)
        .click();
    pageIndexFromEvent = (await removePageEvent).detail;

    assertEquals(expectedPageIndex, pageIndexFromEvent);
  });

  // Verify clicking the rescan page button fires the 'show-rescan-page-dialog'
  // event with the correct page number.
  test('rescanPageClick', async () => {
    assert(actionToolbar);
    const expectedPageIndex = 5;
    let pageIndexFromEvent = -1;

    actionToolbar.pageIndex = expectedPageIndex;
    const rescanPageEvent: Promise<CustomEvent<number>> =
        eventToPromise('show-rescan-page-dialog', actionToolbar);
    strictQuery('#rescanPageIcon', actionToolbar.shadowRoot!, HTMLElement)
        .click();
    pageIndexFromEvent = (await rescanPageEvent).detail;

    assertEquals(expectedPageIndex, pageIndexFromEvent);
  });
});

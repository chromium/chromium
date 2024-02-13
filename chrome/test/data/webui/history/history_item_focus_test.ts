// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/history.js';

import type {HistoryItemElement} from 'chrome://history/history.js';
import {BrowserServiceImpl} from 'chrome://history/history.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestBrowserService} from './test_browser_service.js';
import {createHistoryEntry} from './test_util.js';

suite('<history-item> focus test', function() {
  let item: HistoryItemElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserServiceImpl.setInstance(new TestBrowserService());

    item = document.createElement('history-item');
    item.item = createHistoryEntry('2016-03-16 10:00', 'http://www.google.com');
    document.body.appendChild(item);
    return waitAfterNextRender(item);
  });

  test('refocus checkbox on click', async () => {
    await flushTasks();
    item.$['menu-button'].focus();
    assertEquals(item.$['menu-button'], item.shadowRoot!.activeElement);

    const whenCheckboxSelected =
        eventToPromise('history-checkbox-select', item);
    item.$['time-accessed'].click();

    await whenCheckboxSelected;
    assertEquals(item.$.checkbox, item.shadowRoot!.activeElement);
  });

  test('RemovingBookmarkMovesFocus', async () => {
    item.item = Object.assign({}, item.item, {starred: true});
    await flushTasks();

    // Mimic using tab keys to move focus to the bookmark star. This is needed
    // to allow FocusRowBehavior to realize focus has already been moved into
    // the item. Otherwise, FocusRowBehavior will see that it newly received
    // focus and attempt to move the focus to the first focusable item since
    // the bookmark star is not in the focus order.
    item.$.checkbox.focus();
    item.$.link.focus();
    const star = item.shadowRoot!.querySelector<HTMLElement>('#bookmark-star');
    assertTrue(!!star);
    star.focus();
    star.click();

    // Check that focus is shifted to overflow menu icon.
    assertEquals(item.shadowRoot!.activeElement, item.$['menu-button']);
  });
});

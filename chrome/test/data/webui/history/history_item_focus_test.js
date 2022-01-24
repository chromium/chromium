// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserService} from 'chrome://history/history.js';
import {TestBrowserService} from 'chrome://test/history/test_browser_service.js';
import {createHistoryEntry} from 'chrome://test/history/test_util.js';
import {eventToPromise, flushTasks, waitAfterNextRender} from 'chrome://test/test_util.js';

suite('<history-item> focus test', function() {
  let item;

  setup(function() {
    document.body.innerHTML = '';
    BrowserService.setInstance(new TestBrowserService());

    item = document.createElement('history-item');
    item.item = createHistoryEntry('2016-03-16 10:00', 'http://www.google.com');
    document.body.appendChild(item);
    return waitAfterNextRender(item);
  });

  test('refocus checkbox on click', async () => {
    await flushTasks();
    item.$['menu-button'].focus();
    assertEquals(item.$['menu-button'], item.root.activeElement);

    const whenCheckboxSelected =
        eventToPromise('history-checkbox-select', item);
    item.$['time-accessed'].click();

    await whenCheckboxSelected;
    assertEquals(item.$['checkbox'], item.root.activeElement);
  });
});

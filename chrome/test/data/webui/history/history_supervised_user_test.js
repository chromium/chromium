// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserService, ensureLazyLoaded} from 'chrome://history/history.js';
import {TestBrowserService} from 'chrome://test/history/test_browser_service.js';
import {createHistoryEntry, createHistoryInfo} from 'chrome://test/history/test_util.js';
import {flushTasks} from 'chrome://test/test_util.js';

suite('history-list supervised-user', function() {
  let app;
  let historyList;
  let toolbar;
  let testService;
  const TEST_HISTORY_RESULTS =
      [createHistoryEntry('2016-03-15', 'https://www.google.com')];

  setup(function() {
    document.body.innerHTML = '';
    testService = new TestBrowserService();
    BrowserService.setInstance(testService);

    testService.setQueryResult({
      info: createHistoryInfo(),
      value: TEST_HISTORY_RESULTS,
    });
    app = document.createElement('history-app');
    document.body.appendChild(app);

    historyList = app.$.history;
    toolbar = app.$.toolbar;
    return Promise.all([
      testService.whenCalled('queryHistory'),
      ensureLazyLoaded(),
    ]);
  });

  test('checkboxes disabled for supervised user', function() {
    return flushTasks().then(function() {
      const items = historyList.shadowRoot.querySelectorAll('history-item');

      items[0].$['checkbox'].click();

      assertFalse(items[0].selected);
    });
  });

  test('deletion disabled for supervised user', function() {
    // Make sure that removeVisits is not being called.
    historyList.historyData_[0].selected = true;
    toolbar.deleteSelectedItems();
    assertEquals(0, testService.getCallCount('removeVisits'));
  });

  test('remove history menu button disabled', function() {
    const listContainer = app.$['history'];
    listContainer.$.sharedMenu.get();
    assertTrue(
        listContainer.shadowRoot.querySelector('#menuRemoveButton').hidden);
  });
});

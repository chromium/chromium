// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {HistoryAppElement, HistoryEntry, HistoryListElement, HistoryToolbarElement} from 'chrome://history/history.js';
import {BrowserServiceImpl, ensureLazyLoaded} from 'chrome://history/history.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestBrowserService} from './test_browser_service.js';
import {createHistoryEntry, createHistoryInfo} from './test_util.js';

suite('history-list supervised-user', function() {
  let app: HistoryAppElement;
  let historyList: HistoryListElement;
  let toolbar: HistoryToolbarElement;
  let testService: TestBrowserService;
  const TEST_HISTORY_RESULTS: HistoryEntry[] =
      [createHistoryEntry('2016-03-15', 'https://www.google.com')];

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testService = new TestBrowserService();
    BrowserServiceImpl.setInstance(testService);

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
      const items = historyList.shadowRoot!.querySelectorAll('history-item');

      items[0]!.$.checkbox.click();

      assertFalse(items[0]!.selected);
    });
  });

  test('deletion disabled for supervised user', function() {
    return flushTasks()
        .then(function() {
          const whenChecked =
              eventToPromise('history-checkbox-select', historyList);
          // Manually dispatch the event since the checkboxes are disabled due
          // to the test configuration.
          historyList.shadowRoot!.querySelector('history-item')!.dispatchEvent(
              new CustomEvent('history-checkbox-select', {
                bubbles: true,
                composed: true,
                detail: {index: 0, shiftKey: false},
              }));
          return whenChecked;
        })
        .then(() => {
          toolbar.deleteSelectedItems();
          // Make sure that removeVisits is not being called.
          assertEquals(0, testService.getCallCount('removeVisits'));
        });
  });

  test('remove history menu button disabled', function() {
    historyList.$.sharedMenu.get();
    assertTrue(
        historyList.shadowRoot!.querySelector<HTMLElement>(
                                   '#menuRemoveButton')!.hidden);
  });
});

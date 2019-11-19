// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('history-list supervised-user', function() {
  let app;
  let historyList;
  let toolbar;
  let TEST_HISTORY_RESULTS;

  suiteSetup(function() {
    TEST_HISTORY_RESULTS =
        [createHistoryEntry('2016-03-15', 'https://www.google.com')];
  });

  setup(function() {
    app = replaceApp();
    historyList = app.$.history;
    toolbar = app.$.toolbar;
    app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);
  });

  test('checkboxes disabled for supervised user', function() {
    return test_util.flushTasks().then(function() {
      const items = historyList.shadowRoot.querySelectorAll('history-item');

      MockInteractions.tap(items[0].$['checkbox']);

      assertFalse(items[0].selected);
    });
  });

  test('deletion disabled for supervised user', function() {
    // Make sure that removeVisits is not being called.
    registerMessageCallback('removeVisits', this, function(toBeRemoved) {
      assertNotReached();
    });

    historyList.historyData_[0].selected = true;
    toolbar.deleteSelectedItems();
  });

  test('remove history menu button disabled', function() {
    const listContainer = app.$['history'];
    listContainer.$.sharedMenu.get();
    assertTrue(listContainer.$$('#menuRemoveButton').hidden);
  });
});

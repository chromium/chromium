// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const TEST_HISTORY_RESULTS = [
  createHistoryEntry('2016-03-16 10:00', 'http://www.google.com'),
  createHistoryEntry('2016-03-16 9:00', 'http://www.example.com'),
  createHistoryEntry('2016-03-16 7:01', 'http://www.badssl.com'),
  createHistoryEntry('2016-03-16 7:00', 'http://www.website.com'),
  createHistoryEntry('2016-03-16 4:00', 'http://www.website.com'),
  createHistoryEntry('2016-03-15 11:00', 'http://www.example.com'),
];

const SEARCH_HISTORY_RESULTS = [
  createSearchEntry('2016-03-16', 'http://www.google.com'),
  createSearchEntry('2016-03-14 11:00', 'http://calendar.google.com'),
  createSearchEntry('2016-03-14 10:00', 'http://mail.google.com')
];

suite('<history-item> unit test', function() {
  let item;

  setup(function() {
    item = document.createElement('history-item');
    item.item = TEST_HISTORY_RESULTS[0];
    replaceBody(item);
  });

  test('click targets for selection', function() {
    let selectionCount = 0;
    item.addEventListener('history-checkbox-select', function() {
      selectionCount++;
    });

    // Checkbox should trigger selection.
    MockInteractions.tap(item.$.checkbox);
    assertEquals(1, selectionCount);

    // Non-interactive text should trigger selection.
    MockInteractions.tap(item.$['time-accessed']);
    assertEquals(2, selectionCount);

    // Menu button should not trigger selection.
    MockInteractions.tap(item.$['menu-button']);
    assertEquals(2, selectionCount);
  });

  test('refocus checkbox on click', function() {
    return test_util.flushTasks().then(function() {
      item.$['menu-button'].focus();
      assertEquals(item.$['menu-button'], item.root.activeElement);

      MockInteractions.tap(item.$['time-accessed']);
      assertEquals(item.$['checkbox'], item.root.activeElement);
    });
  });

  test('title changes with item', function() {
    const time = item.$['time-accessed'];
    assertEquals('', time.title);

    time.dispatchEvent(new CustomEvent('mouseover'));
    const initialTitle = time.title;
    item.item = TEST_HISTORY_RESULTS[5];
    time.dispatchEvent(new CustomEvent('mouseover'));
    assertNotEquals(initialTitle, time.title);
  });
});

suite('<history-item> integration test', function() {
  let element;

  setup(function() {
    element = replaceApp().$.history;
  });

  test('basic separator insertion', function() {
    element.addNewResults(TEST_HISTORY_RESULTS);
    return test_util.flushTasks().then(function() {
      Polymer.dom.flush();
      // Check that the correct number of time gaps are inserted.
      const items = element.shadowRoot.querySelectorAll('history-item');

      assertTrue(items[0].hasTimeGap);
      assertTrue(items[1].hasTimeGap);
      assertFalse(items[2].hasTimeGap);
      assertTrue(items[3].hasTimeGap);
      assertFalse(items[4].hasTimeGap);
      assertFalse(items[5].hasTimeGap);
    });
  });

  test('separator insertion for search', function() {
    element.addNewResults(SEARCH_HISTORY_RESULTS);
    element.searchedTerm = 'search';

    return test_util.flushTasks().then(function() {
      Polymer.dom.flush();
      const items = element.shadowRoot.querySelectorAll('history-item');

      assertTrue(items[0].hasTimeGap);
      assertFalse(items[1].hasTimeGap);
      assertFalse(items[2].hasTimeGap);
    });
  });

  test('separator insertion after deletion', function() {
    element.addNewResults(TEST_HISTORY_RESULTS);
    return test_util.flushTasks().then(function() {
      Polymer.dom.flush();
      const items = element.shadowRoot.querySelectorAll('history-item');

      element.removeItemsByIndex_([3]);
      assertEquals(5, element.historyData_.length);

      // Checks that a new time gap separator has been inserted.
      assertTrue(items[2].hasTimeGap);

      element.removeItemsByIndex_([3]);

      // Checks time gap separator is removed.
      assertFalse(items[2].hasTimeGap);
    });
  });

  test('remove bookmarks', function() {
    element.addNewResults(TEST_HISTORY_RESULTS);
    return test_util.flushTasks()
        .then(function() {
          element.set('historyData_.1.starred', true);
          element.set('historyData_.5.starred', true);
          return test_util.flushTasks();
        })
        .then(function() {
          items = element.shadowRoot.querySelectorAll('history-item');

          items[1].$$('#bookmark-star').focus();
          MockInteractions.tap(items[1].$$('#bookmark-star'));

          // Check that focus is shifted to overflow menu icon.
          assertEquals(items[1].root.activeElement, items[1].$['menu-button']);
          // Check that all items matching this url are unstarred.
          assertEquals(element.historyData_[1].starred, false);
          assertEquals(element.historyData_[5].starred, false);
        });
  });
});

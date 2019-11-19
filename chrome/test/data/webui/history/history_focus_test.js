// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for History which are run as interactive ui tests.
 * Should be used for tests which care about focus.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

function HistoryFocusTest() {}

HistoryFocusTest.prototype = {
  __proto__: PolymerInteractiveUITest.prototype,

  browsePreload: 'chrome://history',

  extraLibraries: [
    ...PolymerInteractiveUITest.prototype.extraLibraries,
    '../test_util.js',
    'test_util.js',
  ],

  setUp: function() {
    PolymerInteractiveUITest.prototype.setUp.call(this);

    suiteSetup(function() {
      // Wait for the top-level app element to be upgraded.
      return waitForAppUpgrade()
          .then(() => history.ensureLazyLoaded())
          .then(() => {
            $('history-app').queryState_.queryingDisabled = true;
          });
    });
  },
};

TEST_F('HistoryFocusTest', 'All', function() {
  suite('<history-toolbar>', function() {
    let app;
    let toolbar;

    setup(function() {
      window.resultsRendered = false;
      app = replaceApp();

      toolbar = app.$['toolbar'];
    });

    test('search bar is focused on load in wide mode', function() {
      toolbar.$['main-toolbar'].narrow = false;

      historyResult(createHistoryInfo(), []);
      return test_util.flushTasks().then(() => {
        // Ensure the search bar is focused on load.
        assertTrue(
            app.$.toolbar.$['main-toolbar'].getSearchField().isSearchFocused());
      });
    });

    test('search bar is not focused on load in narrow mode', function() {
      toolbar.$['main-toolbar'].narrow = true;

      historyResult(createHistoryInfo(), []);
      return test_util.flushTasks().then(() => {
        // Ensure the search bar is focused on load.
        assertFalse($('history-app')
                        .$.toolbar.$['main-toolbar']
                        .getSearchField()
                        .isSearchFocused());
      });
    });

    test('shortcuts to open search field', function() {
      const field = toolbar.$['main-toolbar'].getSearchField();
      field.blur();
      assertFalse(field.showingSearch);

      const modifier = cr.isMac ? 'meta' : 'ctrl';
      MockInteractions.pressAndReleaseKeyOn(document.body, 70, modifier, 'f');
      assertTrue(field.showingSearch);
      assertEquals(field.$.searchInput, field.root.activeElement);

      MockInteractions.pressAndReleaseKeyOn(
          field.$.searchInput, 27, '', 'Escape');
      assertFalse(field.showingSearch, 'Pressing escape closes field.');
      assertNotEquals(field.$.searchInput, field.root.activeElement);
    });
  });

  suite('<history-list>', function() {
    let app;
    let element;
    let TEST_HISTORY_RESULTS;

    suiteSetup(function() {
      TEST_HISTORY_RESULTS = [
        createHistoryEntry('2016-03-15', 'https://www.google.com'),
        createHistoryEntry('2016-03-14 10:00', 'https://www.example.com'),
        createHistoryEntry('2016-03-14 9:00', 'https://www.google.com'),
        createHistoryEntry('2016-03-13', 'https://en.wikipedia.org')
      ];
      TEST_HISTORY_RESULTS[2].starred = true;
    });

    setup(function() {
      app = replaceApp();
      element = app.$.history;
      return test_util.flushTasks();
    });

    test('list focus and keyboard nav', async () => {
      app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);
      let focused;
      await test_util.flushTasks();
      Polymer.dom.flush();
      const items = polymerSelectAll(element, 'history-item');

      items[2].$.checkbox.focus();
      focused = items[2].$.checkbox.getFocusableElement();

      // Wait for next render to ensure that focus handlers have been
      // registered (see HistoryItemElement.attached).
      await test_util.waitAfterNextRender(this);

      MockInteractions.pressAndReleaseKeyOn(focused, 39, [], 'ArrowRight');
      Polymer.dom.flush();
      focused = items[2].$.link;
      assertEquals(focused, element.lastFocused_);
      assertTrue(items[2].row_.isActive());
      assertFalse(items[3].row_.isActive());

      MockInteractions.pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
      Polymer.dom.flush();
      focused = items[3].$.link;
      assertEquals(focused, element.lastFocused_);
      assertFalse(items[2].row_.isActive());
      assertTrue(items[3].row_.isActive());

      MockInteractions.pressAndReleaseKeyOn(focused, 39, [], 'ArrowRight');
      Polymer.dom.flush();
      focused = items[3].$['menu-button'];
      assertEquals(focused, element.lastFocused_);
      assertFalse(items[2].row_.isActive());
      assertTrue(items[3].row_.isActive());

      MockInteractions.pressAndReleaseKeyOn(focused, 38, [], 'ArrowUp');
      Polymer.dom.flush();
      focused = items[2].$['menu-button'];
      assertEquals(focused, element.lastFocused_);
      assertTrue(items[2].row_.isActive());
      assertFalse(items[3].row_.isActive());

      MockInteractions.pressAndReleaseKeyOn(focused, 37, [], 'ArrowLeft');
      Polymer.dom.flush();
      focused = items[2].$$('#bookmark-star');
      assertEquals(focused, element.lastFocused_);
      assertTrue(items[2].row_.isActive());
      assertFalse(items[3].row_.isActive());

      MockInteractions.pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
      Polymer.dom.flush();
      focused = items[3].$.link;
      assertEquals(focused, element.lastFocused_);
      assertFalse(items[2].row_.isActive());
      assertTrue(items[3].row_.isActive());
    });
  });

  suite('<history-synced-device-manager>', function() {
    let element;

    setup(function() {
      element = document.createElement('history-synced-device-manager');
      element.signInState = true;
      element.searchTerm = '';
      replaceBody(element);
    });

    test('focus and keyboard nav', function() {
      const sessionList = [
        createSession('Nexus 5', [createWindow([
                        'http://www.example.com', 'http://www.google.com'
                      ])]),
        createSession('Pixel C', [createWindow(['http://www.badssl.com'])]),
        createSession('Potato', [createWindow(['http://www.wikipedia.org'])]),
      ];

      element.sessionList = sessionList;

      let lastFocused;
      let cards;
      let focused;
      const onFocusHandler = element.focusGrid_.onFocus;
      element.focusGrid_.onFocus = function(row, e) {
        onFocusHandler.call(element.focusGrid_, row, e);
        lastFocused = e.currentTarget;
      };

      return test_util.flushTasks()
          .then(function() {
            cards = polymerSelectAll(element, 'history-synced-device-card');

            focused = cards[0].$['menu-button'];
            focused.focus();

            // Go to the collapse button.
            MockInteractions.pressAndReleaseKeyOn(
                focused, 39, [], 'ArrowRight');
            focused = cards[0].$['collapse-button'];
            assertEquals(focused, lastFocused);

            // Go to the first url.
            MockInteractions.pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
            focused = polymerSelectAll(cards[0], '.website-link')[0];
            assertEquals(focused, lastFocused);

            // Collapse the first card.
            MockInteractions.pressAndReleaseKeyOn(focused, 38, [], 'ArrowUp');
            focused = cards[0].$['collapse-button'];
            assertEquals(focused, lastFocused);
            MockInteractions.tap(focused);
          })
          .then(function() {
            // Pressing down goes to the next card.
            MockInteractions.pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
            focused = cards[1].$['collapse-button'];
            assertEquals(focused, lastFocused);

            // Expand the first card.
            MockInteractions.pressAndReleaseKeyOn(focused, 38, [], 'ArrowUp');
            focused = cards[0].$['collapse-button'];
            assertEquals(focused, lastFocused);
            MockInteractions.tap(focused);
          })
          .then(function() {
            // First card's urls are focusable again.
            MockInteractions.pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
            focused = polymerSelectAll(cards[0], '.website-link')[0];
            assertEquals(focused, lastFocused);

            // Remove the second URL from the first card.
            sessionList[0].windows[0].tabs.splice(1, 1);
            element.sessionList = sessionList.slice();
            return test_util.flushTasks();
          })
          .then(function() {
            cards = polymerSelectAll(element, 'history-synced-device-card');

            // Go to the next card's menu buttons.
            MockInteractions.pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
            focused = cards[1].$['collapse-button'];
            assertEquals(focused, lastFocused);

            MockInteractions.pressAndReleaseKeyOn(focused, 38, [], 'ArrowUp');
            focused = polymerSelectAll(cards[0], '.website-link')[0];
            assertEquals(focused, lastFocused);

            // Remove the second card.
            sessionList.splice(1, 1);
            element.sessionList = sessionList.slice();
            return test_util.flushTasks();
          })
          .then(function() {
            cards = polymerSelectAll(element, 'history-synced-device-card');

            // Pressing down goes to the next card.
            MockInteractions.pressAndReleaseKeyOn(focused, 40, [], 'ArrowDown');
            focused = cards[1].$['collapse-button'];
            assertEquals(focused, lastFocused);
          });
    });
  });

  mocha.run();
});

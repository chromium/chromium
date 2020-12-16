// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserService} from 'chrome://history/history.js';
import {isMac} from 'chrome://resources/js/cr.m.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {TestBrowserService} from 'chrome://test/history/test_browser_service.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

suite('<history-toolbar>', function() {
  let app;
  let toolbar;

  setup(function() {
    document.body.innerHTML = '';
    window.history.replaceState({}, '', '/');
    BrowserService.instance_ = new TestBrowserService();

    app = document.createElement('history-app');
    document.body.appendChild(app);

    toolbar = app.$['toolbar'];
  });

  test('search bar is focused on load in wide mode', async () => {
    toolbar.$['main-toolbar'].narrow = false;

    await flushTasks();

    // Ensure the search bar is focused on load.
    assertTrue(
        app.$.toolbar.$['main-toolbar'].getSearchField().isSearchFocused());
  });

  test('search bar is not focused on load in narrow mode', async () => {
    toolbar.$['main-toolbar'].narrow = true;

    await flushTasks();
    // Ensure the search bar is focused on load.
    assertFalse(toolbar.$['main-toolbar'].getSearchField().isSearchFocused());
  });

  test('shortcuts to open search field', function() {
    const field = toolbar.$['main-toolbar'].getSearchField();
    field.blur();
    assertFalse(field.showingSearch);

    const modifier = isMac ? 'meta' : 'ctrl';
    pressAndReleaseKeyOn(document.body, 70, modifier, 'f');
    assertTrue(field.showingSearch);
    assertEquals(field.$.searchInput, field.root.activeElement);

    pressAndReleaseKeyOn(field.$.searchInput, 27, '', 'Escape');
    assertFalse(field.showingSearch, 'Pressing escape closes field.');
    assertNotEquals(field.$.searchInput, field.root.activeElement);
  });
});

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserService} from 'chrome://history/history.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {TestBrowserService} from 'chrome://test/history/test_browser_service.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

suite('routing-test', function() {
  let app;
  let list;
  let sidebar;
  let toolbar;

  function navigateTo(route) {
    window.history.replaceState({}, '', route);
    window.dispatchEvent(new CustomEvent('location-changed'));
    // Update from the URL synchronously.
    app.$$('history-router').flushDebouncer('parseUrl');
  }

  setup(function() {
    window.history.replaceState({}, '', '/');
    document.body.innerHTML = '';
    BrowserService.instance_ = new TestBrowserService();
    app = document.createElement('history-app');
    document.body.appendChild(app);

    assertEquals('chrome://history/', window.location.href);
    sidebar = app.$['content-side-bar'];
    toolbar = app.$['toolbar'];
    return flushTasks();
  });

  test('changing route changes active view', function() {
    assertEquals('history', app.$.content.selected);
    navigateTo('/syncedTabs');
    return flushTasks().then(function() {
      assertEquals('syncedTabs', app.$.content.selected);
      assertEquals('chrome://history/syncedTabs', window.location.href);
    });
  });

  test('route updates from sidebar', function() {
    const menu = sidebar.$.menu;
    assertEquals('history', app.selectedPage_);
    assertEquals('chrome://history/', window.location.href);

    menu.children[1].click();
    assertEquals('syncedTabs', app.selectedPage_);
    assertEquals('chrome://history/syncedTabs', window.location.href);

    keyDownOn(menu.children[0], 32, '', 'Space');
    assertEquals('history', app.selectedPage_);
    assertEquals('chrome://history/', window.location.href);
  });

  test('search updates from route', function() {
    assertEquals('chrome://history/', window.location.href);
    const searchTerm = 'Mei';
    assertEquals('history', app.$.content.selected);
    navigateTo('/?q=' + searchTerm);
    assertEquals(searchTerm, toolbar.searchTerm);
  });

  test('route updates from search', function() {
    const searchTerm = 'McCree';
    assertEquals('history', app.$.content.selected);
    app.fire('change-query', {search: searchTerm});
    assertEquals('chrome://history/?q=' + searchTerm, window.location.href);
  });

  test('search preserved across menu items', function() {
    const searchTerm = 'Soldier76';
    const menu = sidebar.$.menu;
    assertEquals('history', app.selectedPage_);
    navigateTo('/?q=' + searchTerm);

    menu.children[1].click();
    assertEquals('syncedTabs', app.selectedPage_);
    assertEquals(searchTerm, toolbar.searchTerm);
    assertEquals(
        'chrome://history/syncedTabs?q=' + searchTerm, window.location.href);

    menu.children[0].click();
    assertEquals('history', app.selectedPage_);
    assertEquals(searchTerm, toolbar.searchTerm);
    assertEquals('chrome://history/?q=' + searchTerm, window.location.href);
  });
});

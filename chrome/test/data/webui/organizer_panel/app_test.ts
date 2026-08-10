// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://organizer-panel.top-chrome/app.js';

import type {OrganizerPanelAppElement} from 'chrome://organizer-panel.top-chrome/app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('OrganizerPanelAppTest', () => {
  let app: OrganizerPanelAppElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.resetForTesting({
      clearSearch: 'Clear search',
      searchTabs: 'Search Tabs',
      shortcutText: 'Ctrl+Shift+A',
    });
    app = document.createElement('organizer-panel-app');
    document.body.appendChild(app);
    await microtasksFinished();
  });

  test('renders search field with correct label and shortcut', () => {
    const searchField = app.shadowRoot.querySelector('cr-toolbar-search-field');
    assertTrue(!!searchField);
    assertEquals('Search Tabs', searchField.getAttribute('label'));
    assertEquals('Clear search', searchField.getAttribute('clear-label'));

    const shortcut = searchField.querySelector('#shortcut');
    assertTrue(!!shortcut);
    assertEquals('Ctrl+Shift+A', shortcut.textContent);
  });
});

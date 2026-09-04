// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TabGroupsDelegate} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('TabGroupsDelegateTest', () => {
  let delegate: TabGroupsDelegate;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.resetForTesting({
      tabGroups: 'Tab Groups',
    });
    delegate = new TabGroupsDelegate();
  });

  test('returns header', () => {
    assertEquals('Tab Groups', delegate.getHeader());
  });

  test('returns tab groups', async () => {
    const items = await delegate.getItems();
    assertEquals(3, items.length);
  });
});

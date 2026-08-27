// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OpenTabsDelegate} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('OpenTabsDelegateTest', () => {
  let delegate: OpenTabsDelegate;

  setup(() => {
    delegate = new OpenTabsDelegate();
  });

  test('returns header', () => {
    assertEquals('Open Tabs', delegate.getHeader());
  });

  test('returns mock items', () => {
    const items = delegate.getItems();
    assertEquals(3, items.length);
    assertEquals('Google', items[0]!.title);
    assertEquals('google.com', items[0]!.description);
    assertEquals('https://www.google.com', items[0]!.prefixIcon?.urls?.[0]);
  });
});

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {getShortcutSearchHandler, setShortcutSearchHandlerForTesting} from 'chrome://shortcut-customization/js/search/shortcut_search_handler.js';
import {ShortcutSearchHandlerInterface} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('shortcutSearchHandlerTest', function() {
  test('SettingGettingTestHandler', () => {
    // TODO(longbowei): Replace with fake when built.
    const fake_handler: ShortcutSearchHandlerInterface = new Object();
    setShortcutSearchHandlerForTesting(fake_handler);
    assertEquals(fake_handler, getShortcutSearchHandler());
  });
});

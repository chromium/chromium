// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {FakeShortcutSearchHandler} from 'chrome://shortcut-customization/js/search/fake_shortcut_search_handler.js';
import {getShortcutSearchHandler, setShortcutSearchHandlerForTesting} from 'chrome://shortcut-customization/js/search/shortcut_search_handler.js';
import {ShortcutSearchHandlerInterface} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('shortcutSearchHandlerTest', function() {
  test('SettingGettingTestSearchHandler', () => {
    const fake_handler: ShortcutSearchHandlerInterface =
        new FakeShortcutSearchHandler();
    setShortcutSearchHandlerForTesting(fake_handler);
    assertEquals(fake_handler, getShortcutSearchHandler());
  });

  test('GetDefaultSearchHandler', () => {
    const handler = getShortcutSearchHandler();
    assertTrue(!!handler);
  });
});

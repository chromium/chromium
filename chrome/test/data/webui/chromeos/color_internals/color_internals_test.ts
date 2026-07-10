// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {whenAttributeIs, whenCheck} from 'chrome://webui-test/test_util.js';

suite('ColorInternalsTest', () => {
  setup(async () => {
    const table = document.querySelector('table');
    assert(!!table);
    assert(!!table.tBodies[0]);
    await whenCheck(
        table, () => !!(table.tBodies[0] && table.tBodies[0].rows.length > 0));
    const block = document.getElementById('wallpaper-block');
    assert(!!block);
    await whenAttributeIs(block, 'loading', null);
  });

  test('HasChromeSchemeURL', () => {
    const HOST_ORIGIN = 'chrome://color-internals';
    assertEquals(HOST_ORIGIN, document.location.origin);
  });

  test('BuildsTokenTable', () => {
    const table = document.querySelector('table');
    assert(!!table);
    assert(!!table.tBodies[0]);
    assertNotEquals(0, table.tBodies[0].rows.length);
  });

  test('DisplaysWallpaperColors', () => {
    const kMeanContainer =
        document.getElementById('wallpaper-k-mean-color-container');
    assert(!!kMeanContainer);
    assertEquals(
        1, kMeanContainer.querySelectorAll('.wallpaper-color-container').length,
        'one k mean color should be displayed');

    const celebiContainer =
        document.getElementById('wallpaper-celebi-color-container');
    assert(!!celebiContainer);
    assertEquals(
        1,
        celebiContainer.querySelectorAll('.wallpaper-color-container').length,
        'one celebi color should be displayed');
  });
});

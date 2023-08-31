// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search.js';

import {WallpaperSearchElement} from 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('WallpaperSearchTest', () => {
  let wallpaperSearchElement: WallpaperSearchElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    wallpaperSearchElement =
        document.createElement('customize-chrome-wallpaper-search');
    document.body.appendChild(wallpaperSearchElement);
  });

  test('wallpaper search element added to side panel', async () => {
    assertTrue(document.body.contains(wallpaperSearchElement));
  });
});

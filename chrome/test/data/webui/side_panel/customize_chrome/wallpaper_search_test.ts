// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search.js';

import {WallpaperSearchElement} from 'chrome://customize-chrome-side-panel.top-chrome/wallpaper_search.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

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

  test('clicking back button creates event', async () => {
    const eventPromise = eventToPromise('back-click', wallpaperSearchElement);
    wallpaperSearchElement.$.heading.getBackButton().click();
    const event = await eventPromise;
    assertTrue(!!event);
  });
});

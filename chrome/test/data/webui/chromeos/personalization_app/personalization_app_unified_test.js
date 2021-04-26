// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WallpaperCollectionsTest} from './wallpaper_collections_element_test.js';
import {WallpaperImagesTest} from './wallpaper_images_element_test.js';

// Mute console.warn during tests. Several tests intentionally hit asserts to
// verify errors are thrown, and fill test logs with misleading stacktraces.
window.console.warn = () => {};

const testCases = [
  WallpaperCollectionsTest,
  WallpaperImagesTest,
];

for (const testCase of testCases) {
  suite(testCase.name, testCase);
}

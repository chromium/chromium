// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import {WallpaperCollectionsTest} from './wallpaper_collections_element_test.js';
import {WallpaperImagesTest} from './wallpaper_images_element_test.js';

const testCases = [
  WallpaperCollectionsTest,
  WallpaperImagesTest,
];

for (const testCase of testCases) {
  suite(testCase.name, testCase);
}

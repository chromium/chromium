// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import './personalization_app.mojom-lite.js';

/**
 * @type {
 *    ?chromeos.personalizationApp.mojom.WallpaperProviderInterface
 * }
 */
let wallpaperProvider = null;

/**
 * @param {
 *    !chromeos.personalizationApp.mojom.WallpaperProviderInterface
 * } testProvider
 */
export function setWallpaperProviderForTesting(testProvider) {
  wallpaperProvider = testProvider;
}

/**
 * @return {
 *    !chromeos.personalizationApp.mojom.WallpaperProviderInterface
 * }
 */
export function getWallpaperProvider() {
  if (wallpaperProvider) {
    return wallpaperProvider;
  }
  wallpaperProvider =
      chromeos.personalizationApp.mojom.WallpaperProvider.getRemote();
  return wallpaperProvider;
}

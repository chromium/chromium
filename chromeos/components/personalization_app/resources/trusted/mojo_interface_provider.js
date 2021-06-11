// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview a singleton getter for the mojom interface used in
 * the Personalization SWA. Also contains utility functions around fetching
 * mojom data and mocking out the implementation for testing.
 */

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import './personalization_app.mojom-lite.js';

/** @type {?chromeos.personalizationApp.mojom.WallpaperProviderInterface} */
let wallpaperProvider = null;

/**
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     testProvider
 */
export function setWallpaperProviderForTesting(testProvider) {
  wallpaperProvider = testProvider;
}

/**
 * Returns a singleton for the WallpaperProvider mojom interface.
 * @return {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 */
export function getWallpaperProvider() {
  if (!wallpaperProvider) {
    wallpaperProvider =
        chromeos.personalizationApp.mojom.WallpaperProvider.getRemote();
  }
  return wallpaperProvider;
}

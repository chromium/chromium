// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview a singleton getter for the mojom interfaces used in
 * the Personalization SWA. Also contains utility functions around fetching
 * mojom data and mocking out the implementation for testing.
 */

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
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

/**
 * A helper function to fetch collections and throw on error.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @return {!Promise<{collections:
 *     !Array<!chromeos.personalizationApp.mojom.WallpaperCollection>,}>}
 */
export async function fetchCollectionsHelper(provider) {
  const {collections} = await provider.fetchCollections();
  if (!Array.isArray(collections)) {
    throw new Error('No collections available')
  }
  return {collections};
}

/**
 * A helper function to fetch collection images and throw on error.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!string} collectionId
 * @return {!Promise<{images:
 *     !Array<!chromeos.personalizationApp.mojom.WallpaperImage>,}>}
 */
export async function fetchImagesForCollectionHelper(provider, collectionId) {
  const {images} = await provider.fetchImagesForCollection(collectionId);
  if (!Array.isArray(images)) {
    throw new Error('No images available');
  }
  return {images};
}

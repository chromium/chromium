// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isNonEmptyArray} from '../common/utils.js';
import {beginLoadImagesForCollectionsAction, beginLoadLocalImageDataAction, beginSelectImageAction, setCollectionsAction, setImagesForCollectionAction, setLocalImageDataAction, setLocalImagesAction, setSelectedImageAction} from './personalization_actions.js';
import {PersonalizationStore} from './personalization_store.js';

/**
 * @fileoverview contains all of the functions to interact with C++ side through
 * mojom calls. Handles setting |PersonalizationStore| state in response to
 * mojom data.
 * TODO(b/181697575) handle errors and allow user to retry these functions.
 */

/**
 * Fetch wallpaper collections and save them to the store.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
async function fetchCollections(provider, store) {
  let {collections} = await provider.fetchCollections();
  if (!isNonEmptyArray(collections)) {
    console.warn('Failed to fetch wallpaper collections');
    collections = null;
  }
  store.dispatch(setCollectionsAction(collections));
}

/**
 * Fetch all of the wallpaper collections one at a time.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
async function fetchAllImagesForCollections(provider, store) {
  const collections = store.data.backdrop.collections;
  if (!Array.isArray(collections)) {
    console.warn(
        'Cannot fetch data for collections when it is not initialized');
    return;
  }
  store.dispatch(beginLoadImagesForCollectionsAction(collections));
  for (const {id} of /** @type {!Array<{id: string}>} */ (collections)) {
    let {images} = await provider.fetchImagesForCollection(id);
    if (!isNonEmptyArray(images)) {
      console.warn('Failed to fetch images for collection id', id);
      images = null;
    }
    store.dispatch(setImagesForCollectionAction(id, images));
  }
}

/**
 * Get list of local images from disk and save it to the store.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
async function getLocalImages(provider, store) {
  const {images} = await provider.getLocalImages();
  if (images == null) {
    console.warn('Failed to fetch local images');
  }
  store.dispatch(setLocalImagesAction(images));
}

/**
 * Get an image thumbnail for every local image one at a time.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
async function getAllLocalImageThumbnails(provider, store) {
  const images = store.data.local.images;
  if (!Array.isArray(images)) {
    console.warn('Cannot fetch thumbnails of null image list');
    return;
  }
  for (const image of images) {
    store.dispatch(beginLoadLocalImageDataAction(image));
  }
  for (const image of images) {
    const {data} = await provider.getLocalImageThumbnail(image.id);
    if (!data) {
      console.warn('Failed to fetch image data', image.name);
    }
    store.dispatch(setLocalImageDataAction(image, data));
  }
}

/**
 * Get the currently set wallpaper.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
export async function getCurrentWallpaper(provider, store) {
  const {image} = await provider.getCurrentWallpaper();
  store.dispatch(setSelectedImageAction(image));
}

/**
 * @param {!chromeos.personalizationApp.mojom.WallpaperImage |
 *     !chromeos.personalizationApp.mojom.LocalImage} image
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
export async function selectWallpaper(image, provider, store) {
  const oldImage = store.data.selected;
  store.dispatch(beginSelectImageAction(image));
  const {success} = await (() => {
    if (image.assetId) {
      return provider.selectWallpaper(image.assetId);
    } else if (image.id) {
      return provider.selectLocalImage(image.id);
    } else {
      console.warn('Image must be a LocalImage or a WallpaperImage');
      return {success: false};
    }
  })();
  if (!success) {
    console.warn('Error setting wallpaper');
  }
  getCurrentWallpaper(provider, store);
}

/**
 * @param {!chromeos.personalizationApp.mojom.WallpaperLayout} layout
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
export async function setCustomWallpaperLayout(layout, provider, store) {
  await provider.setCustomWallpaperLayout(layout);
  getCurrentWallpaper(provider, store);
}

/**
 * Fetches list of collections, then fetches list of images for each collection.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
export async function initializeBackdropData(provider, store) {
  await fetchCollections(provider, store);
  await fetchAllImagesForCollections(provider, store);
}

/**
 * Gets list of local images, then fetches image thumbnails for each local
 * image.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
export async function initializeLocalData(provider, store) {
  await getLocalImages(provider, store);
  await getAllLocalImageThumbnails(provider, store);
}

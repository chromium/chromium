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
 * @return {!Promise<Array<chromeos.personalizationApp.mojom.WallpaperCollection>>}
 */
export async function fetchCollections(provider, store) {
  let {collections} = await provider.fetchCollections();
  if (!isNonEmptyArray(collections)) {
    console.warn('Failed to fetch wallpaper collections');
    collections = null;
  }
  store.dispatch(setCollectionsAction(collections));
  return collections;
}

/**
 * Fetch all of the wallpaper collections one at a time.
 * @param {!Array<!chromeos.personalizationApp.mojom.WallpaperCollection>}
 *     collections
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
export async function fetchAllImagesForCollections(
    collections, provider, store) {
  store.dispatch(beginLoadImagesForCollectionsAction(collections));
  for (const {id} of collections) {
    let {images} = await provider.fetchImagesForCollection(id);
    if (!isNonEmptyArray(images)) {
      console.warn('Failed to fetch images for collection id', id);
      images = null;
    }
    store.dispatch(setImagesForCollectionAction(id, images));
  }
}

/**
 * Get list of local images from disk.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
export async function getLocalImages(provider, store) {
  const {images} = await provider.getLocalImages();
  if (images == null) {
    console.warn('Failed to fetch local images');
  }
  store.dispatch(setLocalImagesAction(images));
}

/**
 * Get an image thumbnail for each image one at a time.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
export async function getAllLocalImageThumbnails(provider, store) {
  const images = store.data.local.images;
  if (!Array.isArray(images)) {
    console.warn('Cannot fetch thumbnails when local image list is null');
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
 * @param {!chromeos.personalizationApp.mojom.WallpaperImage} image
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
export async function selectWallpaper(image, provider, store) {
  const oldImage = store.data.selected;
  store.dispatch(beginSelectImageAction(image));
  const {success} = await provider.selectWallpaper(image.assetId);
  if (!success) {
    console.warn('Error setting wallpaper');
  }
  const newImage = success ? image : oldImage;
  store.dispatch(setSelectedImageAction(newImage));
}

/**
 * Fetches list of collections, then fetches list of images for each collection.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     provider
 * @param {!PersonalizationStore} store
 */
export async function initializeBackdropData(provider, store) {
  const collections = await fetchCollections(provider, store);
  if (!Array.isArray(collections)) {
    return;
  }
  await fetchAllImagesForCollections(collections, provider, store);
}

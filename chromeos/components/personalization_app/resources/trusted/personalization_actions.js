// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/js/cr/ui/store.m.js';
import {unguessableTokenToString} from '../common/utils.js';
import {DisplayableImage} from './personalization_reducers.js';

/**
 * @fileoverview Defines the actions to change state.
 */

/** @enum {string} */
export const ActionName = {
  BEGIN_LOAD_IMAGES_FOR_COLLECTIONS: 'begin_load_images_for_collections',
  BEGIN_LOAD_LOCAL_IMAGE_DATA: 'begin_load_local_image_data',
  BEGIN_SELECT_IMAGE: 'begin_select_image',
  SET_COLLECTIONS: 'set_collections',
  SET_IMAGES_FOR_COLLECTION: 'set_images_for_collection',
  SET_LOCAL_IMAGES: 'set_local_images',
  SET_LOCAL_IMAGE_DATA: 'set_local_image_data',
  SET_SELECTED_IMAGE: 'set_selected_image',
};


/**
 * Notify that app is loading image list for the given collection.
 * @param {?Array<!chromeos.personalizationApp.mojom.WallpaperCollection>}
 *     collections
 * @return {!Action}
 */
export function beginLoadImagesForCollectionsAction(collections) {
  return {
    collections,
    name: ActionName.BEGIN_LOAD_IMAGES_FOR_COLLECTIONS,
  };
}

/**
 * Notify that app is loading thumbnail for the given local image.
 * @param {!chromeos.personalizationApp.mojom.LocalImage} image
 * @return {!Action}
 */
export function beginLoadLocalImageDataAction(image) {
  return {
    id: unguessableTokenToString(image.id),
    name: ActionName.BEGIN_LOAD_LOCAL_IMAGE_DATA,
  };
}

/**
 * Notify that a user has clicked on an image to set as wallpaper.
 * @param {!DisplayableImage} image
 * @return {!Action}
 */
export function beginSelectImageAction(image) {
  return {name: ActionName.BEGIN_SELECT_IMAGE, image};
}

/**
 * Set the collections. May be called with null if an error occurred.
 * @param {?Array<!chromeos.personalizationApp.mojom.WallpaperCollection>}
 *     collections
 * @return {!Action}
 */
export function setCollectionsAction(collections) {
  return {
    collections,
    name: ActionName.SET_COLLECTIONS,
  };
}

/**
 * Set the images for a given collection. May be called with null if an error
 * occurred.
 * @param {string} collectionId
 * @param {?Array<!chromeos.personalizationApp.mojom.WallpaperImage>} images
 * @returns
 */
export function setImagesForCollectionAction(collectionId, images) {
  return {
    collectionId,
    images,
    name: ActionName.SET_IMAGES_FOR_COLLECTION,
  };
}

/**
 * Set the thumbnail data for a local image.
 * @param {!chromeos.personalizationApp.mojom.LocalImage} image
 * @param {string} data
 * @return {!Action}
 */
export function setLocalImageDataAction(image, data) {
  return {
    id: unguessableTokenToString(image.id),
    data,
    name: ActionName.SET_LOCAL_IMAGE_DATA,
  };
}

/**
 * Set the list of local images.
 * @param {?Array<!chromeos.personalizationApp.mojom.LocalImage>} images
 * @return {!Action}
 */
export function setLocalImagesAction(images) {
  return {
    images,
    name: ActionName.SET_LOCAL_IMAGES,
  };
}

/**
 * Returns an action to set the current image as currently selected across the
 * app. Can be called with null to represent no image currently selected or that
 * an error occurred.
 * @param {?chromeos.personalizationApp.mojom.CurrentWallpaper} image
 * @return {!Action}
 */
export function setSelectedImageAction(image) {
  return {
    image,
    name: ActionName.SET_SELECTED_IMAGE,
  };
}

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
  BEGIN_LOAD_SELECTED_IMAGE: 'begin_load_selected_image',
  BEGIN_SELECT_IMAGE: 'begin_select_image',
  BEGIN_UPDATE_DAILY_REFRESH_IMAGE: 'begin_update_daily_refresh_image',
  END_SELECT_IMAGE: 'end_select_image',
  SET_COLLECTIONS: 'set_collections',
  SET_DAILY_REFRESH_COLLECTION_ID: 'set_daily_refresh_collection_id',
  SET_IMAGES_FOR_COLLECTION: 'set_images_for_collection',
  SET_LOCAL_IMAGES: 'set_local_images',
  SET_LOCAL_IMAGE_DATA: 'set_local_image_data',
  SET_SELECTED_IMAGE: 'set_selected_image',
  SET_UPDATED_DAILY_REFRESH_IMAGE: 'set_updated_daily_refreshed_image',
  DISMISS_ERROR: 'dismiss_error',
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
 * Notify that a user has clicked on the refresh button.
 * @return {!Action}
 */
export function beginUpdateDailyRefreshImageAction() {
  return {
    name: ActionName.BEGIN_UPDATE_DAILY_REFRESH_IMAGE,
  };
}

/**
 * Notify that app is loading currently selected image information.
 * @return {!Action}
 */
export function beginLoadSelectedImageAction() {
  return {name: ActionName.BEGIN_LOAD_SELECTED_IMAGE};
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
 * Notify that the user-initiated action to set image has finished.
 * @param {!DisplayableImage} image
 * @param {boolean} success
 * @return {!Action}
 */
export function endSelectImageAction(image, success) {
  return {name: ActionName.END_SELECT_IMAGE, image, success};
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
 * Set and enable daily refresh for given collectionId.
 * @param {?string} collectionId
 * @return {!Action}
 */
export function setDailyRefreshCollectionIdAction(collectionId) {
  return {
    collectionId, name: ActionName.SET_DAILY_REFRESH_COLLECTION_ID,
  }
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
 * Notify that a image has been refreshed.
 * @return {!Action}
 */
export function setUpdatedDailyRefreshImageAction() {
  return {
    name: ActionName.SET_UPDATED_DAILY_REFRESH_IMAGE,
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

/**
 * @return {!Action}
 */
export function dismissErrorAction() {
  return {name: ActionName.DISMISS_ERROR};
}

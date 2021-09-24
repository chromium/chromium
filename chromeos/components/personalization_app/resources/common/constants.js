// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview TODO(cowmoo)
 */

export const untrustedOrigin = 'chrome-untrusted://personalization';

export const trustedOrigin = 'chrome://personalization';

export const kMaximumLocalImagePreviews = 3;

/** @enum {string} */
export const EventType = {
  SEND_COLLECTIONS: 'send_collections',
  SELECT_COLLECTION: 'select_collection',
  SELECT_LOCAL_COLLECTION: 'select_local_collection',
  SEND_IMAGE_COUNTS: 'send_image_counts',
  SEND_IMAGES: 'send_images',
  SEND_LOCAL_IMAGE_DATA: 'send_local_image_data',
  SEND_LOCAL_IMAGES: 'send_local_images',
  SEND_CURRENT_WALLPAPER_ASSET_ID: 'send_current_wallpaper_asset_id',
  SEND_PENDING_WALLPAPER_ASSET_ID: 'send_pending_wallpaper_asset_id',
  SELECT_IMAGE: 'select_image',
  SELECT_LOCAL_IMAGE: 'select_local_image',
  SEND_VISIBLE: 'send_visible',
};

/**
 * @typedef {{
 *   type: EventType,
 *   collections:
 *     !Array<!chromeos.personalizationApp.mojom.WallpaperCollection>,
 * }}
 */
export let SendCollectionsEvent;

/**
 * @typedef {{ type: EventType, collectionId: string }}
 */
export let SelectCollectionEvent;

/**
 * @typedef {{ type: EventType }}
 */
export let SelectLocalCollectionEvent;

/**
 * @typedef {{ type: EventType, counts: Object<string, number> }}
 */
export let SendImageCountsEvent;

/**
 * @typedef {{
 *   type: EventType,
 *   images: !Array<!chromeos.personalizationApp.mojom.WallpaperImage>,
 * }}
 */
export let SendImagesEvent;

/**
 * @typedef {{
 *   type: EventType,
 *   images: !Array<!mojoBase.mojom.FilePath>,
 * }}
 */
export let SendLocalImagesEvent;

/**
 * Sends local image data keyed by stringified local image path.
 * @typedef {{
 *   type: EventType,
 *   data: !Object<string, string>,
 * }}
 */
export let SendLocalImageDataEvent;

/**
 * @typedef {{
 *   type: EventType,
 *   assetId: ?bigint,
 * }}
 */
export let SendCurrentWallpaperAssetIdEvent;

/**
 * @typedef {{
 *  type: EventType,
 *  assetId: ?bigint,
 * }}
 */
export let SendPendingWallpaperAssetIdEvent;

/**
 * @typedef {{ type: EventType, assetId: bigint }}
 */
export let SelectImageEvent;

/**
 * Notify an iframe if its visible state changes.
 * @typedef {{ type: EventType, visible: boolean }}
 */
export let SendVisibleEvent;

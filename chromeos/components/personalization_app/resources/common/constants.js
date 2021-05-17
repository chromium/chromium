// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview TODO(cowmoo)
 */

export const untrustedOrigin = 'chrome-untrusted://personalization';

export const trustedOrigin = 'chrome://personalization';

/** @enum {string} */
export const EventType = {
  SEND_COLLECTIONS: 'send_collections',
  SELECT_COLLECTION: 'select_collection',
  SEND_IMAGES: 'send_images',
  SELECT_IMAGE: 'select_image',
};

/**
 * @typedef {{ type: EventType, collections:
 *     !Array<!chromeos.personalizationApp.mojom.WallpaperCollection> }}
 */
export let SendCollectionsEvent;

/**
 * @typedef {{ type: EventType, collectionId: string }}
 */
export let SelectCollectionEvent;

/**
 * @typedef {{ type: EventType, images:
 *     !Array<!chromeos.personalizationApp.mojom.WallpaperImage> }}
 */
export let SendImagesEvent;

/**
 * @typedef {{ type: EventType, assetId: bigint }}
 */
export let SelectImageEvent;

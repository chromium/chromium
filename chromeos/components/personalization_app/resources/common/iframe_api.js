// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '/assert.m.js';
import {EventType, SelectCollectionEvent, SelectImageEvent, SelectLocalCollectionEvent, SendCollectionsEvent, SendImageCountsEvent, SendImagesEvent, SendLocalImageDataEvent, SendLocalImagesEvent, trustedOrigin, untrustedOrigin} from './constants.js';
import {isNonEmptyArray} from './utils.js';

/**
 * @fileoverview Helper functions for communicating between trusted and
 * untrusted. All trusted <-> untrusted communication must happen through the
 * functions in this file.
 */

/****************** Trusted -> Untrusted **************************************/

/**
 * Send an array of wallpaper collections to untrusted.
 * @param {!Object} target the untrusted iframe window to send the message to.
 * @param {!Array<!chromeos.personalizationApp.mojom.WallpaperCollection>}
 *     collections
 */
export function sendCollections(target, collections) {
  /** @type {!SendCollectionsEvent} */
  const event = {type: EventType.SEND_COLLECTIONS, collections};
  target.postMessage(event, untrustedOrigin);
}

/**
 * Send a mapping of collectionId to the number of images in that collection.
 * @param {!Window} target
 * @param {!Object<string, number>} counts
 */
export function sendImageCounts(target, counts) {
  /** @type {!SendImageCountsEvent} */
  const event = {type: EventType.SEND_IMAGE_COUNTS, counts};
  target.postMessage(event, untrustedOrigin);
}

/**
 * Send an array of wallpaper images to chrome-untrusted://.
 * Will clear the page if images is empty array.
 * @param {!Window} target the iframe window to send the message to.
 * @param {!Array<!chromeos.personalizationApp.mojom.WallpaperImage>} images
 */
export function sendImages(target, images) {
  /** @type {!SendImagesEvent} */
  const event = {type: EventType.SEND_IMAGES, images};
  target.postMessage(event, untrustedOrigin);
}

/**
 * Send an array of local images to chrome-untrusted://.
 * @param {!Window} target the iframe window to send the message to.
 * @param {!Array<!chromeos.personalizationApp.mojom.LocalImage>} images
 */
export function sendLocalImages(target, images) {
  /** @type {!SendLocalImagesEvent} */
  const event = {type: EventType.SEND_LOCAL_IMAGES, images};
  target.postMessage(event, untrustedOrigin);
}

/**
 * @param {!Window} target
 * @param {!chromeos.personalizationApp.mojom.LocalImage} image
 * @param {!string} data
 */
export function sendLocalImageData(target, image, data) {
  /** @type {!SendLocalImageDataEvent} */
  const event = {type: EventType.SEND_LOCAL_IMAGE_DATA, id: image.id, data};
  target.postMessage(event, untrustedOrigin);
}

/**
 * Called from trusted code to validate that a received postMessage event
 * contains valid data. Ignores messages that are not of the expected type.
 * @param {!Event} event from untrusted to select a collection or image
 * @param {Array<!T>} choices array of valid objects to pick from
 * @return {!T}
 * @template T
 */
export function validateReceivedSelection(event, choices) {
  assert(isNonEmptyArray(choices), 'choices must be a non-empty array');

  /** @type {SelectCollectionEvent|SelectImageEvent} */
  const data = event.data;
  let selected;
  switch (data.type) {
    case EventType.SELECT_COLLECTION:
      assert(!!data.collectionId, 'Expected a collection id parameter');
      selected = choices.find(choice => choice.id === data.collectionId);
      break;
    case EventType.SELECT_IMAGE:
      assert(
          data.hasOwnProperty('assetId'),
          'Expected an image assetId parameter');
      assert(
          typeof data.assetId === 'bigint', 'assetId parameter must be bigint');
      selected = choices.find(choice => choice.assetId === data.assetId);
      break;
    default:
      assertNotReached('Unknown event type');
  }

  assert(!!selected, 'No valid selection found in choices');
  return selected;
}

/****************** Untrusted -> Trusted **************************************/

/**
 * Select a collection. Sent from untrusted to trusted.
 * @param {!Object} target the window object to post the message to.
 * @param {!string} collectionId the selected collection id.
 */
export function selectCollection(target, collectionId) {
  /** @type {!SelectCollectionEvent} */
  const event = {type: EventType.SELECT_COLLECTION, collectionId};
  target.postMessage(event, trustedOrigin);
}

export function selectLocalCollection(target) {
  /** @type {!SelectLocalCollectionEvent} */
  const event = {type: EventType.SELECT_LOCAL_COLLECTION};
  target.postMessage(event, trustedOrigin);
}

/**
 * Select an image. Sent from untrusted to trusted.
 * @param {!Object} target the window to post the message to.
 * @param {!bigint} assetId the selected image assetId.
 */
export function selectImage(target, assetId) {
  /** @type {!SelectImageEvent} */
  const event = {type: EventType.SELECT_IMAGE, assetId};
  target.postMessage(event, trustedOrigin);
}

/**
 * Called from untrusted code to validate that a received event is of an
 * expected type and contains the expected data.
 * @param {!Event} event
 * @param {!EventType} expectedEventType
 * @return {!Array<!T>}
 * @template T
 */
export function validateReceivedData(event, expectedEventType) {
  assert(
      event.origin === trustedOrigin, 'Message is not from the correct origin');
  assert(
      event.data.type === expectedEventType,
      `Expected event type: ${expectedEventType}`);

  /** @type {SendCollectionsEvent|SendImagesEvent} */
  const data = event.data;
  switch (data.type) {
    case EventType.SEND_COLLECTIONS:
      assert(isNonEmptyArray(data.collections), 'Expected collections array');
      return data.collections;
    case EventType.SEND_LOCAL_IMAGES:
    case EventType.SEND_IMAGES:
      // Images array may be empty.
      assert(Array.isArray(data.images), 'Expected images array');
      return data.images;
    default:
      assertNotReached('Unknown event type');
  }
  return [];
}

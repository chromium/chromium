// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventType} from '../common/constants.js';
import {selectImage, validateReceivedData} from '../common/iframe_api.js';

/**
 * @fileoverview Responds to |SendImagesEvent| from trusted. Handles user input
 * and responds with |SelectImageEvent| when an image is selected.
 */

/** @param {Event} event */
function onClickImage(event) {
  // This throws if assetId is not a valid BigInt.
  const assetId = BigInt(event.currentTarget.dataset.assetId);
  selectImage(window.parent, assetId);
}

/**
 * @param {!chromeos.personalizationApp.mojom.WallpaperImage} image
 * @returns {!HTMLImageElement}
 */
function imageToHtml(image) {
  const img = /** @type {!HTMLImageElement} */ (document.createElement('img'));
  img.src = image.url.url;
  img.dataset.assetId = image.assetId;
  img.onclick = onClickImage;
  return img;
}

/** @param {!Array<!chromeos.personalizationApp.mojom.WallpaperImage>} images */
function onImagesReceived(images) {
  while (document.body.firstChild) {
    document.body.removeChild(document.body.firstChild);
  }
  for (const image of images) {
    document.body.appendChild(imageToHtml(image));
  }
}

/**
 * Handler for messages from trusted code. Expects only SendImagesEvent and will
 * error on any other event.
 * @param {!Event} message
 */
function onMessageReceived(message) {
  const images = validateReceivedData(message, EventType.SEND_IMAGES);
  onImagesReceived(images);
}

window.addEventListener('message', onMessageReceived);

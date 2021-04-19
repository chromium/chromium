// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventType} from '../common/constants.js';
import {selectCollection, validateReceivedData} from '../common/iframe_api.js';

/**
 * @fileoverview Responds to |SendCollectionsEvent| messages from trusted.
 * Handles user input and replies with |SelectCollectionEvent| back to trusted.
 */

/** @param {Event} event */
function onClickCollection(event) {
  selectCollection(window.parent, event.currentTarget.dataset.id);
}

/**
 * @param {!chromeos.personalizationApp.mojom.WallpaperCollection} collection
 * @return {!HTMLDivElement}
 */
function collectionToHtmlElement(collection) {
  const div = /** @type {!HTMLDivElement} */ (document.createElement('div'));
  div.setAttribute('data-id', collection.id);
  div.onclick = onClickCollection;
  if (collection.preview && collection.preview.url) {
    const img = document.createElement('img');
    img.src = collection.preview.url;
    div.appendChild(img);
  }
  const p = document.createElement('p');
  p.innerText = collection.name;
  div.appendChild(p);
  return div;
}

/**
 * @param {!Array<!chromeos.personalizationApp.mojom.WallpaperCollection>}
 *     collections
 */
function onCollectionsReceived(collections) {
  while (document.body.firstChild) {
    document.body.removeChild(document.body.firstChild);
  }
  for (const collection of collections) {
    document.body.appendChild(collectionToHtmlElement(collection));
  }
}

/**
 * Handler for messages from trusted code. Expects only SendCollectionsEvent and
 * will error on any other event.
 * @param {!Event} message
 */
function onMessageReceived(message) {
  const collections = validateReceivedData(message, EventType.SEND_COLLECTIONS);
  onCollectionsReceived(collections);
}

window.addEventListener('message', onMessageReceived);

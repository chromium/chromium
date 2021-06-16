// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//personalization/polymer/v3_0/iron-list/iron-list.js';
import '../common/styles.js';
import {html, PolymerElement} from 'chrome-untrusted://personalization/polymer/v3_0/polymer/polymer_bundled.min.js';
import {EventType} from '../common/constants.js';
import {selectCollection, selectLocalCollection, validateReceivedData} from '../common/iframe_api.js';
import {unguessableTokenToString} from '../common/utils.js';

/**
 * @fileoverview Responds to |SendCollectionsEvent| from trusted. Handles user
 * input and responds with |SelectCollectionEvent| when an image is selected.
 */

const kLocalCollectionId = 'local_';

/**
 * @typedef {{id: string, name: string, preview: ?url.mojom.Url}}
 */
let Tile;

/**
 * A common display format between local images and WallpaperCollection.
 * Get the first displayable image with data from the list of possible images.
 * TODO(b/184774974) display a collage of up to three images.
 * @param {Array<!chromeos.personalizationApp.mojom.LocalImage>} localImages
 * @param {Object<string, string>} localImageData
 * @return {!Tile}
 */
function getLocalTile(localImages, localImageData) {
  if (localImageData && Array.isArray(localImages)) {
    for (const {id, name} of localImages) {
      const key = unguessableTokenToString(id);
      const data = localImageData[key];
      if (!data) {
        continue;
      }
      return {name, preview: {url: data}, id: kLocalCollectionId};
    }
  }
  // TODO(b/184774974) replace zero state with translated string from UI spec.
  return {name: 'No Images', preview: null, id: kLocalCollectionId};
}

class CollectionsGrid extends PolymerElement {
  static get is() {
    return 'collections-grid';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {!Array<!chromeos.personalizationApp.mojom.WallpaperCollection>}
       * @private
       */
      collections_: {
        type: Array,
      },

      /**
       * @type {!Array<!chromeos.personalizationApp.mojom.LocalImage>}
       * @private
       */
      localImages_: {
        type: Array,
        value: [],
      },

      /**
       * Stores a mapping of local image id to thumbnail data.
       * @private
       * @type {!Object<string, string>}
       */
      localImageData_: {
        type: Object,
        value: {},
      },

      /**
       * @type {!Array<!Tile>}
       */
      tiles_: {
        type: Array,
        computed: 'computeTiles_(collections_, localImages_, localImageData_)',
      },
    };
  }

  /** @override */
  constructor() {
    super();
    this.onMessageReceived_ = this.onMessageReceived_.bind(this);
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    window.addEventListener('message', this.onMessageReceived_);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('message', this.onMessageReceived_);
  }

  /**
   * @param {!Array<!chromeos.personalizationApp.mojom.WallpaperCollection>}
   *     collections
   * @param {!Array<!chromeos.personalizationApp.mojom.LocalImage>} localImages
   */
  computeTiles_(collections, localImages, localImageData) {
    return [getLocalTile(localImages, localImageData), ...(collections || [])];
  }

  /**
   * Handler for messages from trusted code. Expects only SendImagesEvent and
   * will error on any other event.
   * @param {!Event} message
   * @private
   */
  onMessageReceived_(message) {
    switch (message.data.type) {
      case EventType.SEND_COLLECTIONS:
        try {
          this.collections_ =
              validateReceivedData(message, EventType.SEND_COLLECTIONS);
        } catch (e) {
          console.warn('Invalid collections received', e);
          this.collections_ = [];
        }
        break;
      case EventType.SEND_LOCAL_IMAGES:
        try {
          this.localImages_ =
              validateReceivedData(message, EventType.SEND_LOCAL_IMAGES);
        } catch (e) {
          console.warn('Invalid local images received', e);
          this.localImages_ = [];
        }
        break;
      case EventType.SEND_LOCAL_IMAGE_DATA:
        this.localImageData_ = {
          ...this.localImageData_,
          [unguessableTokenToString(message.data.id)]: message.data.data,
        };
        break;
      default:
        console.error(`Unexpected event type ${message.data.type}`);
        break;
    }
  }

  /**
   * Notify trusted code that a user clicked on a collection.
   * @private
   * @param {!Event} e
   */
  onCollectionClicked_(e) {
    const id = e.currentTarget.dataset.id;
    if (id === kLocalCollectionId) {
      selectLocalCollection(window.parent);
      return;
    }
    selectCollection(window.parent, id);
  }
}

customElements.define(CollectionsGrid.is, CollectionsGrid);

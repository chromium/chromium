// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://personalization/polymer/v3_0/iron-list/iron-list.js';
import './setup.js';
import './styles.js';
import {html, PolymerElement} from 'chrome-untrusted://personalization/polymer/v3_0/polymer/polymer_bundled.min.js';
import {EventType, kMaximumLocalImagePreviews} from '../common/constants.js';
import {selectCollection, selectLocalCollection, validateReceivedData} from '../common/iframe_api.js';
import {unguessableTokenToString} from '../common/utils.js';

/**
 * @fileoverview Responds to |SendCollectionsEvent| from trusted. Handles user
 * input and responds with |SelectCollectionEvent| when an image is selected.
 */

const kLocalCollectionId = 'local_';

/**
 * A displayable type constructed from a LocalImage or a WallpaperImage.
 * @typedef {{
 *   id: string,
 *   name: string,
 *   count: string,
 *   preview: !Array<!url.mojom.Url>,
 * }}
 */
let Tile;

/**
 * Get the text to display for number of images.
 * @param {?number|undefined} x
 * @return {string}
 */
function getCountText(x) {
  switch (x) {
    case undefined:
    case null:
      return '';
    case 0:
      return loadTimeData.getString('zeroImages');
    case 1:
      return loadTimeData.getString('oneImage');
    default:
      if (typeof x !== 'number' || x < 0) {
        console.error('Received an impossible value');
        return '';
      }
      return loadTimeData.getStringF('multipleImages', x);
  }
}

/**
 *
 * @param {?Array<!chromeos.personalizationApp.mojom.LocalImage>} localImages
 * @param {Object<string, string>} localImageData
 * @return {!Array<!url.mojom.Url>}
 */
function getImages(localImages, localImageData) {
  if (!localImageData || !Array.isArray(localImages)) {
    return [];
  }
  return localImages
      .map(({id}) => ({url: localImageData[unguessableTokenToString(id)]}))
      .filter((data, index) => {
        // |data.url| may be undefined or empty if this local image thumbnail
        // has not loaded yet.
        return !!data.url && data.url.length > 0 &&
            index < kMaximumLocalImagePreviews;
      });
}

/**
 * A common display format between local images and WallpaperCollection.
 * Get the first displayable image with data from the list of possible images.
 * TODO(b/184774974) display a collage of up to three images.
 * @param {Array<!chromeos.personalizationApp.mojom.LocalImage>} localImages
 * @param {Object<string, string>} localImageData
 * @return {!Tile}
 */
function getLocalTile(localImages, localImageData) {
  const name = loadTimeData.getString('myImagesLabel');
  const imagesToDisplay = getImages(localImages, localImageData);
  return {
    name,
    id: kLocalCollectionId,
    count: getCountText(Array.isArray(localImages) ? localImages.length : 0),
    preview: imagesToDisplay,
  };
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
        value: [],
      },

      /**
       * Mapping of collection id to number of images. Loads in progressively
       * after collections_.
       * @type {!Object<string, number>}
       * @private
       */
      imageCounts_: {
        type: Object,
        value: {},
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
        computed:
            'computeTiles_(collections_, imageCounts_, localImages_, localImageData_)',
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
   * @param {!Object<string, number>} imageCounts
   * @param {!Array<!chromeos.personalizationApp.mojom.LocalImage>} localImages
   * @param {!Object<string, string>} localImageData
   */
  computeTiles_(collections, imageCounts, localImages, localImageData) {
    const localTile = getLocalTile(localImages, localImageData);
    const collectionTiles = collections.map(({name, id, preview}) => {
      return {
        name,
        id,
        count: getCountText(imageCounts[id]),
        preview: [preview],
      };
    });
    return [localTile, ...collectionTiles];
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
      case EventType.SEND_IMAGE_COUNTS:
        this.imageCounts_ = message.data.counts;
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
   * @param {!Tile} tile
   * @return {string}
   */
  getClassForImagesContainer_(tile) {
    const numImages = Array.isArray(tile?.preview) ? tile.preview.length : 0;
    return `photo-images-container photo-images-container-${
        Math.min(numImages, kMaximumLocalImagePreviews)}`;
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

  /**
   * @private
   * @param {number} i
   * @return {number}
   */
  getAriaIndex_(i) {
    return i + 1;
  }
}

customElements.define(CollectionsGrid.is, CollectionsGrid);

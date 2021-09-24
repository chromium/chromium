// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://personalization/polymer/v3_0/iron-list/iron-list.js';
import './setup.js';
import './styles.js';
import {afterNextRender, html, PolymerElement} from 'chrome-untrusted://personalization/polymer/v3_0/polymer/polymer_bundled.min.js';
import {EventType, kMaximumLocalImagePreviews} from '../common/constants.js';
import {selectCollection, selectLocalCollection, validateReceivedData} from '../common/iframe_api.js';
import {getLoadingPlaceholderAnimationDelay, isSelectionEvent} from '../common/utils.js';

/**
 * @fileoverview Responds to |SendCollectionsEvent| from trusted. Handles user
 * input and responds with |SelectCollectionEvent| when an image is selected.
 */

const kLocalCollectionId = 'local_';

/** Width in pixels of when the app switches from 3 to 4 tiles wide. */
const k3to4WidthCutoffPx = 688;

/** Height in pixels of a tile. */
const kTileHeightPx = 136;

/** @enum {string} */
const TileType = {
  loading: 'loading',
  image: 'image',
  failure: 'failure',
};

/**
 * @typedef {{type: TileType}}
 */
let LoadingTile;

/**
 * Type that represents a collection that failed to load. The preview image
 * is still displayed, but is grayed out and unclickable.
 * @typedef {{
 *   id: string,
 *   name: string,
 *   preview: !Array<!url.mojom.Url>,
 *   type: TileType,
 * }}
 */
let FailureTile;

/**
 * A displayable type constructed from up to three LocalImages or a
 * WallpaperCollection.
 * @typedef {{
 *   id: string,
 *   name: string,
 *   count: string,
 *   preview: !Array<!url.mojom.Url>,
 *   type: TileType,
 * }}
 */
let ImageTile;

/** @typedef {LoadingTile|FailureTile|ImageTile} */
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
 * @param {?Array<!mojoBase.mojom.FilePath>} localImages
 * @param {Object<string, string>} localImageData
 * @return {!Array<!url.mojom.Url>}
 */
function getImages(localImages, localImageData) {
  if (!localImageData || !Array.isArray(localImages)) {
    return [];
  }
  const result = [];
  for (const {path} of localImages) {
    const data = {url: localImageData[path]};
    if (!!data.url && data.url.length > 0) {
      result.push(data);
    }
    // Add at most |kMaximumLocalImagePreviews| thumbnail urls.
    if (result.length >= kMaximumLocalImagePreviews) {
      break;
    }
  }
  return result;
}

/**
 * A common display format between local images and WallpaperCollection.
 * Get the first displayable image with data from the list of possible images.
 * @param {!Array<!mojoBase.mojom.FilePath>} localImages
 * @param {!Object<string, string>} localImageData
 * @return {!ImageTile|!LoadingTile}
 */
function getLocalTile(localImages, localImageData) {
  const isMoreToLoad =
      localImages.some(({path}) => !localImageData.hasOwnProperty(path));

  const imagesToDisplay = getImages(localImages, localImageData);

  if (imagesToDisplay.length < kMaximumLocalImagePreviews && isMoreToLoad) {
    // If there are more images to attempt loading thumbnails for, wait until
    // those are done.
    return {type: TileType.loading};
  }

  // Count all images that failed to load and subtract them from "My Images"
  // count.
  const failureCount = Object.values(localImageData).reduce((result, next) => {
    return next === '' ? result + 1 : result;
  }, 0);

  return {
    name: loadTimeData.getString('myImagesLabel'),
    id: kLocalCollectionId,
    count: getCountText(
        Array.isArray(localImages) ? localImages.length - failureCount : 0),
    preview: imagesToDisplay,
    type: TileType.image,
  };
}

export class CollectionsGrid extends PolymerElement {
  static get is() {
    return 'collections-grid';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {Array<!chromeos.personalizationApp.mojom.WallpaperCollection>}
       * @private
       */
      collections_: {
        type: Array,
      },

      /**
       * Mapping of collection id to number of images. Loads in progressively
       * after collections_.
       * @type {Object<string, number>}
       * @private
       */
      imageCounts_: {
        type: Object,
      },

      /**
       * @type {Array<!mojoBase.mojom.FilePath>}
       * @private
       */
      localImages_: {
        type: Array,
      },

      /**
       * Stores a mapping of local image id to thumbnail data.
       * @type {Object<string, string>}
       * @private
       */
      localImageData_: {
        type: Object,
      },

      /**
       * List of tiles to be displayed to the user.
       * @type {!Array<!Tile>}
       * @private
       */
      tiles_: {
        type: Array,
        value() {
          // Fill the view with loading tiles. Will be adjusted to the correct
          // number of tiles when collections are received.
          const x = window.innerWidth > k3to4WidthCutoffPx ? 4 : 3;
          const y = Math.floor(window.innerHeight / kTileHeightPx);
          return Array.from({length: x * y}, () => ({type: TileType.loading}));
        }
      },
    };
  }

  static get observers() {
    return [
      'onLocalImagesLoaded_(localImages_, localImageData_)',
      'onCollectionLoaded_(collections_, imageCounts_)',
    ]
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
   * Called each time a new collection finishes loading. |imageCounts| contains
   * a mapping of collection id to the number of images in that collection.
   * A value of null indicates that the given collection id has failed to load.
   * @private
   * @param {?Array<!chromeos.personalizationApp.mojom.WallpaperCollection>}
   *     collections
   * @param {?Object<string, ?number>} imageCounts
   */
  onCollectionLoaded_(collections, imageCounts) {
    if (!Array.isArray(collections) || !imageCounts) {
      return;
    }

    while (this.tiles_.length < collections.length + 1) {
      this.push('tiles_', {type: TileType.loading});
    }
    while (this.tiles_.length > collections.length + 1) {
      this.pop('tiles_');
    }

    collections.forEach((collection, i) => {
      const index = i + 1;
      const tile = this.tiles_[index];
      // This tile failed to load completely.
      if (imageCounts[collection.id] === null && !this.isFailureTile_(tile)) {
        this.set(`tiles_.${index}`, {
          id: collection.id,
          name: collection.name,
          count: '',
          preview: [collection.preview],
          type: TileType.failure,
        });
        return;
      }
      // This tile loaded successfully.
      if (typeof imageCounts[collection.id] === 'number' &&
          !this.isImageTile_(tile)) {
        this.set(`tiles_.${index}`, {
          id: collection.id,
          name: collection.name,
          count: getCountText(imageCounts[collection.id]),
          preview: [collection.preview],
          type: TileType.image,
        });
      }
    });
  }

  /**
   * Called with updated local image list or local image thumbnail data when
   * either of those properties changes.
   * @param {?Array<!mojoBase.mojom.FilePath>} localImages
   * @param {Object<string, string>} localImageData
   * @private
   */
  onLocalImagesLoaded_(localImages, localImageData) {
    if (!Array.isArray(localImages) || !localImageData) {
      return;
    }
    const tile = getLocalTile(localImages, localImageData);
    this.set('tiles_.0', tile);
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
          this.localImageData_ = {};
        }
        break;
      case EventType.SEND_LOCAL_IMAGE_DATA:
        try {
          this.localImageData_ =
              validateReceivedData(message, EventType.SEND_LOCAL_IMAGE_DATA);
        } catch (e) {
          console.warn('Invalid local image data received', e);
          this.localImages_ = [];
          this.localImageData_ = {};
        }
        break;
      case EventType.SEND_VISIBLE:
        const visible = validateReceivedData(message, EventType.SEND_VISIBLE);
        if (visible) {
          // If iron-list items were updated while this iron-list was hidden,
          // the layout will be incorrect. Trigger another layout when iron-list
          // becomes visible again. Wait until |afterNextRender| completes
          // otherwise iron-list width may still be 0.
          afterNextRender(this, () => {
            // Trigger a layout now that iron-list has the correct width.
            this.shadowRoot.querySelector('iron-list').fire('iron-resize');
          });
        }
        return;
      default:
        console.error(`Unexpected event type ${message.data.type}`);
        break;
    }
  }

  /**
   * @param {!ImageTile} tile
   * @return {string}
   */
  getClassForImagesContainer_(tile) {
    const numImages = Array.isArray(tile?.preview) ? tile.preview.length : 0;
    return `photo-images-container photo-images-container-${
        Math.min(numImages, kMaximumLocalImagePreviews)}`;
  }

  /**
   * Notify trusted code that a user selected a collection.
   * @private
   * @param {!Event} e
   */
  onCollectionSelected_(e) {
    if (!isSelectionEvent(e)) {
      return;
    }
    const id = e.currentTarget.dataset.id;
    if (id === kLocalCollectionId) {
      selectLocalCollection(window.parent);
      return;
    }
    selectCollection(window.parent, id);
  }

  /**
   * Not using I18nBehavior because of chrome-untrusted:// incompatibility.
   * @param {string} str
   * @return {string}
   */
  geti18n_(str) {
    return loadTimeData.getString(str);
  }

  /**
   * @private
   * @param {?Tile} item
   * @return {boolean}
   */
  isLoadingTile_(item) {
    return item?.type === TileType.loading;
  }

  /**
   * @private
   * @param {?Tile} item
   * @return {boolean}
   */
  isFailureTile_(item) {
    return item?.type === TileType.failure;
  }

  /**
   * @param {?Tile} item
   * @return {boolean}
   * @private
   */
  isEmptyTile_(item) {
    return !!item && item.type === TileType.image && item.preview.length === 0;
  }

  /**
   * @private
   * @param {?Tile} item
   * @return {boolean}
   */
  isImageTile_(item) {
    return item?.type === TileType.image && !this.isEmptyTile_(item);
  }

  /**
   * @param {number} index
   * @return {string}
   */
  getLoadingPlaceholderAnimationDelay(index) {
    return getLoadingPlaceholderAnimationDelay(index);
  }

  /**
   * Make the text and background gradient visible again after the image has
   * finished loading. This is called for both on-load and on-error, as either
   * event should make the text visible again.
   * @param {!Event} event
   * @private
   */
  onImgLoad_(event) {
    const parent = event.currentTarget.closest('.photo-inner-container');
    for (const elem of parent.querySelectorAll('[hidden]')) {
      elem.removeAttribute('hidden');
    }
  }
}

customElements.define(CollectionsGrid.is, CollectionsGrid);

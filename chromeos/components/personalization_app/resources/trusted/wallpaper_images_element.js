// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview WallpaperImages displays a list of wallpaper images from a
 * wallpaper collection. It requires a parameter collection-id to fetch
 * and display the images. It also caches the list of wallpaper images by
 * wallpaper collection id to avoid refetching data unnecessarily.
 */

import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './styles.js';
import {afterNextRender, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {sendCurrentWallpaperAssetId, sendImages, sendPendingWallpaperAssetId, sendVisible} from '../common/iframe_api.js';
import {isNonEmptyArray, promisifyOnload} from '../common/utils.js';
import {DisplayableImage, WallpaperType} from './personalization_reducers.js';
import {WithPersonalizationStore} from './personalization_store.js';

let sendImagesFunction = sendImages;

export function promisifySendImagesForTesting() {
  let resolver;
  const promise = new Promise((resolve) => resolver = resolve);
  sendImagesFunction = (...args) => resolver(args);
  return promise;
}

/**
 * If |current| is set and is an online wallpaper, return the assetId of that
 * image. Otherwise returns null.
 * @param {?chromeos.personalizationApp.mojom.CurrentWallpaper} current
 * @return {?bigint}
 */
function getAssetId(current) {
  if (current?.type !== WallpaperType.kOnline) {
    return null;
  }
  try {
    return BigInt(current.key);
  } catch (e) {
    console.warn('Required a BigInt value here', e);
    return null;
  }
}

/** @polymer */
export class WallpaperImages extends WithPersonalizationStore {
  static get is() {
    return 'wallpaper-images';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Hidden state of this element. Used to notify iframe of visibility
       * changes.
       */
      hidden: {
        type: Boolean,
        reflectToAttribute: true,
        observer: 'onHiddenChanged_',
      },

      /**
       * The current collection id to display.
       */
      collectionId: {
        type: String,
      },

      /**
       * @type {?Array<!chromeos.personalizationApp.mojom.WallpaperCollection>}
       */
      collections_: {
        type: Array,
      },

      /**
       * @type {!Object<string,
       *     ?Array<!chromeos.personalizationApp.mojom.WallpaperImage>>}
       * @private
       */
      images_: {
        type: Object,
      },

      /**
       * Mapping of collection_id to boolean.
       * @type {!Object<string, boolean>}
       */
      imagesLoading_: {
        type: Object,
      },

      /**
       * @type {?chromeos.personalizationApp.mojom.CurrentWallpaper}
       */
      currentSelected_: {
        type: Object,
      },

      /**
       * The pending selected image.
       * @type {?DisplayableImage}
       * @private
       */
      pendingSelected_: {
        type: Object,
      },

      /** @private */
      hasError_: {
        type: Boolean,
        // Call computed functions with their dependencies as arguments so that
        // polymer knows when to re-run the computation.
        computed: 'computeHasError_(images_, imagesLoading_, collectionId)',
      },

      /** @private */
      showImages_: {
        type: Boolean,
        computed: 'computeShowImages_(images_, imagesLoading_, collectionId)',
      },
    };
  }

  static get observers() {
    return [
      'onShouldSendImages_(showImages_, collectionId)',
      'onShouldSendCurrentAssetId_(showImages_, currentSelected_)',
      'onShouldSendPendingAssetId_(pendingSelected_)',
    ]
  }

  constructor() {
    super();
    this.iframePromise_ = /** @type {!Promise<!HTMLIFrameElement>} */ (
        promisifyOnload(this, 'images-iframe', afterNextRender));
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.watch('images_', state => state.backdrop.images);
    this.watch('imagesLoading_', state => state.loading.images);
    this.watch('collections_', state => state.backdrop.collections);
    this.watch('currentSelected_', state => state.currentSelected);
    this.watch('pendingSelected_', state => state.pendingSelected);
    this.updateFromStore();
  }

  /**
   * Notify iframe that this element visibility has changed.
   * @param {boolean} hidden
   * @private
   */
  async onHiddenChanged_(hidden) {
    const iframe = await this.iframePromise_;
    sendVisible(/** @type {!Window} */ (iframe.contentWindow), !hidden);
  }

  /**
   * Check if this collection with id |collectionid| is still loading.
   * @param {?Object<string, boolean>} imagesLoading
   * @param {?string} collectionId
   * @return {boolean}
   */
  isLoading_(imagesLoading, collectionId) {
    if (!imagesLoading || !collectionId) {
      return true;
    }
    return imagesLoading[collectionId];
  }

  /**
   * @private
   * @param {?Object<string,
   *     Array<!chromeos.personalizationApp.mojom.WallpaperImage>>} images
   * @param {?Object<string, boolean>} imagesLoading
   * @param {string} collectionId
   * @return {boolean}
   */
  computeHasError_(images, imagesLoading, collectionId) {
    return !this.isLoading_(imagesLoading, collectionId) &&
        !isNonEmptyArray(images[collectionId]);
  }

  /**
   * @private
   * @param {?Object<string,
   *     Array<!chromeos.personalizationApp.mojom.WallpaperImage>>} images
   * @param {Object<string, boolean>} imagesLoading
   * @param {string} collectionId
   * @return {boolean}
   */
  computeShowImages_(images, imagesLoading, collectionId) {
    return !this.isLoading_(imagesLoading, collectionId) &&
        isNonEmptyArray(images[collectionId]);
  }

  /**
   * Send images if loading is ready and we have some images.
   * @param {boolean} showImages
   * @param {string} collectionId
   */
  async onShouldSendImages_(showImages, collectionId) {
    if (showImages && collectionId) {
      const iframe = await this.iframePromise_;
      sendImagesFunction(iframe.contentWindow, this.images_[collectionId]);
    }
  }

  /**
   * @param {boolean} showImages
   * @param {?chromeos.personalizationApp.mojom.CurrentWallpaper}
   *     currentSelected
   */
  async onShouldSendCurrentAssetId_(showImages, currentSelected) {
    if (showImages) {
      const assetId = getAssetId(currentSelected);
      const iframe = await this.iframePromise_;
      sendCurrentWallpaperAssetId(
          /** @type {!Window} */ (iframe.contentWindow), assetId);
    }
  }

  /**
   * @param {?DisplayableImage} pendingSelected
   */
  async onShouldSendPendingAssetId_(pendingSelected) {
    if (!pendingSelected || !pendingSelected.assetId)
      return;
    const iframe = await this.iframePromise_;
    sendPendingWallpaperAssetId(
        /** @type {!Window} */ (iframe.contentWindow), pendingSelected.assetId);
  }

  /**
   * @private
   * @param {string} collectionId
   * @param {?Array<!chromeos.personalizationApp.mojom.WallpaperCollection>}
   *     collections
   * @return {string}
   */
  getMainAriaLabel_(collectionId, collections) {
    if (!collectionId || !Array.isArray(collections)) {
      return '';
    }
    const collection =
        collections.find(collection => collection.id === collectionId);

    if (!collection) {
      console.warn('Did not find collection matching collectionId');
      return '';
    }

    return collection.name;
  }
}

customElements.define(WallpaperImages.is, WallpaperImages);

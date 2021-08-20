// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview WallpaperImages displays a list of wallpaper images from a
 * wallpaper collection. It requires a parameter collection-id to fetch
 * and display the images. It also caches the list of wallpaper images by
 * wallpaper collection id to avoid refetching data unnecessarily.
 */

import './styles.js';
import {afterNextRender, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {sendCurrentWallpaperAssetId, sendImages, sendPendingWallpaperAssetId, sendVisible} from '../common/iframe_api.js';
import {isNonEmptyArray, promisifyOnload} from '../common/utils.js';
import {DisplayableImage, WallpaperType} from './personalization_reducers.js';
import {PersonalizationRouter} from './personalization_router_element.js';
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

/**
 * Get the loading status of this page.
 * If collections are still loading, or if the specific collection with id
 * |collectionId| is still loading, the page is considered to be loading.
 * @param {?boolean} collectionsLoading
 * @param {?Object<string, boolean>} imagesLoading
 * @param {?string} collectionId
 * @return {boolean}
 * @private
 */
function isLoading(collectionsLoading, imagesLoading, collectionId) {
  if (!imagesLoading || !collectionId) {
    return true;
  }
  return collectionsLoading || (imagesLoading[collectionId] !== false);
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

      /** @private */
      collectionsLoading_: {
        type: Boolean,
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
        observer: 'onCurrentSelectedChanged_',
      },

      /**
       * The pending selected image.
       * @type {?DisplayableImage}
       * @private
       */
      pendingSelected_: {
        type: Object,
        observer: 'onPendingSelectedChanged_',
      },

      /** @private */
      hasError_: {
        type: Boolean,
        // Call computed functions with their dependencies as arguments so that
        // polymer knows when to re-run the computation.
        computed:
            'computeHasError_(images_, imagesLoading_, collections_, collectionsLoading_, collectionId)',
      },

      /**
       * In order to prevent re-sending images every time a collection loads in
       * the background, calculate this intermediate boolean. That way
       * |onImagesUpdated_| will re-run whenever this value flips from false to
       * true, rather than each time a new collection is changed in the
       * background.
       * @private
       */
      hasImages_: {
        type: Boolean,
        computed: 'computeHasImages_(images_, imagesLoading_, collectionId)',
      },
    };
  }

  static get observers() {
    return [
      'onImagesUpdated_(hasImages_, hasError_, collectionId)',
    ]
  }

  /** @override */
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
    this.watch('collectionsLoading_', state => state.loading.collections);
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
    if (!hidden) {
      this.shadowRoot.getElementById('main').focus();
    }
    const iframe = await this.iframePromise_;
    sendVisible(/** @type {!Window} */ (iframe.contentWindow), !hidden);
  }

  /**
   * @param {?chromeos.personalizationApp.mojom.CurrentWallpaper} selected
   * @private
   */
  async onCurrentSelectedChanged_(selected) {
    const assetId = getAssetId(selected);
    const iframe = await this.iframePromise_;
    sendCurrentWallpaperAssetId(
        /** @type {!Window} */ (iframe.contentWindow), assetId);
  }

  /**
   * @param {?DisplayableImage} pendingSelected
   * @private
   */
  async onPendingSelectedChanged_(pendingSelected) {
    const iframe = await this.iframePromise_;
    sendPendingWallpaperAssetId(
        /** @type {!Window} */ (iframe.contentWindow),
        pendingSelected?.assetId || null);
  }

  /**
   * Determine whether the current collection failed to load or is not a valid
   * |collectionId|. Check that collections list loaded successfully, and that
   * the collection with id |collectionId| also loaded successfully.
   * @param {?Object<string,
   *     Array<!chromeos.personalizationApp.mojom.WallpaperImage>>} images
   * @param {?Object<string, boolean>} imagesLoading
   * @param {?Array<!chromeos.personalizationApp.mojom.WallpaperCollection>}
   *     collections
   * @param {boolean} collectionsLoading
   * @param {string} collectionId
   * @return {boolean}
   * @private
   */
  computeHasError_(
      images, imagesLoading, collections, collectionsLoading, collectionId) {
    // Not yet initialized or still loading.
    if (!imagesLoading || !collectionId || collectionsLoading) {
      return false;
    }

    // Failed to load collections or unknown collectionId.
    if (!isNonEmptyArray(collections) ||
        !collections.some(collection => collection.id === collectionId)) {
      return true;
    }

    // Specifically check === false to guarantee that key is in the object and
    // set as false.
    return imagesLoading[collectionId] === false &&
        !isNonEmptyArray(images[collectionId]);
  }

  /**
   * @param {?Object<string,
   *     Array<!chromeos.personalizationApp.mojom.WallpaperImage>>} images
   * @param {Object<string, boolean>} imagesLoading
   * @param {string} collectionId
   * @return {boolean}
   * @private
   */
  computeHasImages_(images, imagesLoading, collectionId) {
    return !!images && !!imagesLoading &&
        // Specifically check === false again here.
        imagesLoading[collectionId] === false &&
        isNonEmptyArray(images[collectionId]);
  }

  /**
   * Send images if loading is ready and we have some images. Punt back to
   * main page if there is an error viewing this collection.
   * @param {boolean} hasImages
   * @param {boolean} hasError
   * @param {string} collectionId
   * @private
   */
  async onImagesUpdated_(hasImages, hasError, collectionId) {
    if (hasError) {
      console.warn('An error occurred while loading collections or images');
      // Navigate back to main page and refresh.
      PersonalizationRouter.reloadAtRoot();
      return;
    }

    if (hasImages && collectionId) {
      const iframe = await this.iframePromise_;
      sendImagesFunction(iframe.contentWindow, this.images_[collectionId]);
    }
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

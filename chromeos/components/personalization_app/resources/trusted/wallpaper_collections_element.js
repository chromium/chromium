// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Polymer element that fetches and displays a list of WallpaperCollection
 * objects.
 */

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './styles.js';
import {afterNextRender, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {sendCollections, sendImageCounts, sendLocalImageData, sendLocalImages} from '../common/iframe_api.js';
import {isNonEmptyArray, promisifyOnload, unguessableTokenToString} from '../common/utils.js';
import {kMaximumLocalImagePreviews} from '../common/constants.js';
import {getWallpaperProvider} from './mojo_interface_provider.js';
import {initializeBackdropData, initializeLocalData} from './personalization_controller.js';
import {WithPersonalizationStore} from './personalization_store.js';

let sendCollectionsFunction = sendCollections;
let sendImageCountsFunction = sendImageCounts;
let sendLocalImagesFunction = sendLocalImages;
let sendLocalImageDataFunction = sendLocalImageData;

/**
 * Mock out the iframe api functions for testing. Return promises that are
 * resolved when the function is called by |WallpaperCollectionsElement|.
 * @return {{
 *   sendCollections: Promise<?>,
 *   sendImageCounts: Promise<?>,
 *   sendLocalImages: Promise<?>,
 *   sendLocalImageData: Promise<?>,
 * }}
 */
export function promisifyIframeFunctionsForTesting() {
  let resolvers = {};
  const promises = [
    sendCollections, sendImageCounts, sendLocalImages, sendLocalImageData
  ].reduce((result, next) => {
    result[next.name] = new Promise(resolve => resolvers[next.name] = resolve);
    return result;
  }, {});
  sendCollectionsFunction = (...args) => resolvers[sendCollections.name](args);
  sendImageCountsFunction = (...args) => resolvers[sendImageCounts.name](args);
  sendLocalImagesFunction = (...args) => resolvers[sendLocalImages.name](args);
  sendLocalImageDataFunction = (...args) =>
      resolvers[sendLocalImageData.name](args);
  return promises;
}

/** @polymer */
export class WallpaperCollections extends WithPersonalizationStore {
  static get is() {
    return 'wallpaper-collections';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @private
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
       * Contains a mapping of collection id to an array of images.
       * @private
       * @type {?Object<string,
       *     ?Array<!chromeos.personalizationApp.mojom.WallpaperImage>>}
       */
      images_: {
        type: Object,
        observer: 'onImagesChanged_',
      },

      /**
       * @private
       * @type {Array<!chromeos.personalizationApp.mojom.LocalImage>}
       */
      localImages_: {
        type: Array,
        observer: 'onLocalImagesChanged_',
      },

      /**
       * Stores a mapping of local image id to thumbnail data.
       * @private
       * @type {Object<string, string>}
       */
      localImageData_: {
        type: Object,
        observer: 'onLocalImageDataChanged_',
      },

      /** @private */
      hasError_: {
        type: Boolean,
        // Call computed functions with their dependencies as arguments so that
        // polymer knows when to re-run the computation.
        computed: 'computeHasError_(collections_, collectionsLoading_)',
      },

      /** @private */
      showCollections_: {
        type: Boolean,
        computed: 'computeShowCollections_(collections_, collectionsLoading_)',
        observer: 'onShowCollectionsChanged_',
      },
    };
  }

  static get observers() {
    return ['onLocalImageDataChanged_(localImages_, localImageData_)'];
  }

  constructor() {
    super();
    /** @private */
    this.wallpaperProvider_ = getWallpaperProvider();
    this.iframePromise_ = /** @type {!Promise<!HTMLIFrameElement>} */ (
        promisifyOnload(this, 'collections-iframe', afterNextRender));

    /**
     * Stores a set of local image ids that have already sent thumbnail data to
     * untrusted.
     * @type {!Set<string>}
     */
    this.sentLocalImages_ = new Set();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.watch('collections_', state => state.backdrop.collections);
    this.watch('collectionsLoading_', state => state.loading.collections);
    this.watch('images_', state => state.backdrop.images);
    this.watch('localImages_', state => state.local.images);
    this.watch('localImageData_', state => state.local.data);
    this.updateFromStore();
    const store = this.getStore();
    initializeBackdropData(this.wallpaperProvider_, store);
    initializeLocalData(this.wallpaperProvider_, store);
  }

  /**
   * @private
   * @param {?Array<!chromeos.personalizationApp.mojom.WallpaperCollection>}
   *     collections
   * @param {boolean} loading
   * @return {boolean}
   */
  computeHasError_(collections, loading) {
    return !loading && !this.computeShowCollections_(collections, loading);
  }

  /**
   * @private
   * @param {?Array<!chromeos.personalizationApp.mojom.WallpaperCollection>}
   *     collections
   * @param {boolean} loading
   * @return {boolean}
   */
  computeShowCollections_(collections, loading) {
    return !loading && isNonEmptyArray(collections);
  }

  /**
   * Send updated wallpaper collections to the iframe.
   * @param {?boolean} value
   */
  async onShowCollectionsChanged_(value) {
    if (value) {
      const iframe = await this.iframePromise_;
      sendCollectionsFunction(iframe.contentWindow, this.collections_);
    }
  }

  /**
   * Send count of images in each collection when a new collection is fetched.
   * @param {?Object<string,
   *     ?Array<!chromeos.personalizationApp.mojom.WallpaperImage>>} value
   */
  async onImagesChanged_(value) {
    if (value == undefined) {
      return;
    }
    const iframe = await this.iframePromise_;
    const counts =
        Object.entries(value)
            .filter(([_, value]) => Array.isArray(value))
            .map(([key, value]) => [key, value.length])
            .reduce(
                (result, [key, value]) => Object.assign(result, {[key]: value}),
                {});
    sendImageCountsFunction(
        /** @type {!Window} */ (iframe.contentWindow), counts);
  }

  /**
   * Send updated local images list to the iframe.
   * @param {?Array<!chromeos.personalizationApp.mojom.LocalImage>} value
   */
  async onLocalImagesChanged_(value) {
    if (Array.isArray(value)) {
      const iframe = await this.iframePromise_;
      sendLocalImagesFunction(
          /** @type {!Window} */ (iframe.contentWindow), value);
    }
  }

  /**
   * Send up to |maximumImageThumbnailsCount| image thumbnails to untrusted.
   * @param {?Array<!chromeos.personalizationApp.mojom.LocalImage>} images
   * @param {?Object<string, string>} imageData
   */
  async onLocalImageDataChanged_(images, imageData) {
    if (!Array.isArray(images) || !imageData) {
      return;
    }
    const iframe = await this.iframePromise_;

    for (const image of images) {
      if (this.sentLocalImages_.size >= kMaximumLocalImagePreviews) {
        return;
      }
      const key = unguessableTokenToString(image.id);
      if (this.sentLocalImages_.has(key)) {
        continue;
      }
      const data = imageData[key];
      if (data) {
        sendLocalImageDataFunction(
            /** @type {!Window} */ (iframe.contentWindow), image, data);
        this.sentLocalImages_.add(key);
      }
    }
  }
}

customElements.define(WallpaperCollections.is, WallpaperCollections);

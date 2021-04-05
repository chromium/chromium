// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview WallpaperImages displays a list of wallpaper images from a
 * wallpaper collection. It requires a parameter collection-id to fetch
 * and display the images. It also caches the list of wallpaper images by
 * wallpaper collection id to avoid refetching data unnecessarily.
 */

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {fetchImagesForCollectionHelper, getWallpaperProvider} from './mojo_interface_provider.js';

export class WallpaperImages extends PolymerElement {
  static get is() {
    return 'wallpaper-images';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      collectionId: {
        type: String,
        observer: 'collectionIdChanged_',
      },

      /**
       * @private
       * @type {?Array<!chromeos.personalizationApp.mojom.WallpaperImage>}
       */
      images_: {
        type: Array,
        value: null,
      },

      /** @private */
      isLoading_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      hasError_: {
        type: Boolean,
        computed: 'computeHasError_(images_, isLoading_)',
      },

      /** @private */
      showImages_: {
        type: Boolean,
        computed: 'computeShowImages_(images_, isLoading_)',
      },
    };
  }

  constructor() {
    super();
    this.wallpaperProvider_ = getWallpaperProvider();
    /**
     * @type {!Map<string,
     *     !Array<!chromeos.personalizationApp.mojom.WallpaperImage>>}
     */
    this.cache_ = new Map();
  }

  /**
   * Listener for collectionId property.
   * @private
   * @param {?string} value
   */
  collectionIdChanged_(value) {
    if (!value) {
      this.images_ = null;
      return;
    }

    if (this.cache_.has(value)) {
      this.images_ = this.cache_.get(value);
      return;
    }

    this.fetchImages_(value);
  }

  /**
   * @private
   * @param {string} collectionId
   */
  async fetchImages_(collectionId) {
    assert(
        (typeof collectionId === 'string') && collectionId.length > 0,
        'Collection id parameter is required');

    if (this.cache_.has(collectionId)) {
      this.images_ = this.cache_.get(collectionId);
      return;
    }

    this.setProperties({isLoading_: true, images_: null});
    try {
      const {images} = await fetchImagesForCollectionHelper(
          this.wallpaperProvider_, collectionId);
      this.images_ = images;
      this.cache_.set(collectionId, this.images_);
    } catch (e) {
      // TODO(b/181697575) handle errors and allow user to retry
      console.warn(
          'Fetching wallpaper collection images failed for collection id',
          collectionId, e);
    } finally {
      this.isLoading_ = false;
    }
  }

  /**
   * @private
   * @param {?Array<!chromeos.personalizationApp.mojom.WallpaperImage>} images
   * @param {boolean} loading
   * @return {boolean}
   */
  computeHasError_(images, loading) {
    return !loading && !this.computeShowImages_(images, loading);
  }

  /**
   * @private
   * @param {?Array<!chromeos.personalizationApp.mojom.WallpaperImage>} images
   * @param {boolean} loading
   * @return {boolean}
   */
  computeShowImages_(images, loading) {
    return !loading && Array.isArray(images) && images.length > 0;
  }

  /**
   * @private
   * @param {!chromeos.personalizationApp.mojom.WallpaperImage} image
   * @return {string}
   */
  imageHref_(image) {
    return image.url.url;
  }
}

customElements.define(WallpaperImages.is, WallpaperImages);

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
import './styles.js';
import {assert} from '/assert.m.js';
import {afterNextRender, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {EventType} from '../common/constants.js';
import {sendImages, validateReceivedSelection} from '../common/iframe_api.js';
import {isNonEmptyArray, promisifyOnload} from '../common/utils.js';
import {fetchImagesForCollectionHelper, getWallpaperProvider} from './mojo_interface_provider.js';

let sendImagesFunction = sendImages;

export function promisifySendImagesForTesting() {
  let resolver;
  const promise = new Promise((resolve) => resolver = resolve);
  sendImagesFunction = (...args) => resolver(args);
  return promise;
}

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
        observer: 'onCollectionIdChanged_',
      },

      /**
       * Used to bind/unbind the message listener when this element is toggled.
       * Also hides the element when it is not active.
       */
      active: {
        type: Boolean,
        observer: 'onActiveChanged_',
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
    this.iframePromise_ = /** @type {!Promise<!HTMLIFrameElement>} */ (
        promisifyOnload(this, 'images-iframe', afterNextRender));
    this.onImageSelected_ = this.onImageSelected_.bind(this);
    this.wallpaperProvider_ = getWallpaperProvider();
    /**
     * @type {!Map<string,
     *     !Array<!chromeos.personalizationApp.mojom.WallpaperImage>>}
     */
    this.cache_ = new Map();
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('message', this.onImageSelected_);
  }

  /**
   * Listener for collectionId property.
   * @private
   * @param {?string} value
   */
  async onCollectionIdChanged_(value) {
    this.setProperties({isLoading_: true, images_: null});
    if (!value) {
      this.isLoading_ = false;
      return;
    }

    // Make sure that iframe is fully loaded.
    const iframe = await this.iframePromise_;

    // Fetch images from backend.
    const images = await this.fetchImages_(value);
    // Send images to iframe. If images is null, send empty array to clear the
    // page instead.
    sendImagesFunction(iframe.contentWindow, images || []);

    this.setProperties({isLoading_: false, images_: images});
  }

  /**
   * @private
   * @param {boolean} value
   */
  onActiveChanged_(value) {
    const func = value ? window.addEventListener : window.removeEventListener;
    func('message', this.onImageSelected_);
    this.hidden = !value;
  }

  /**
   * Fetch images from wallpaperProvider. If there is a cached value for the
   * given collectionId, will read from cache instead.
   * Returns null on failure.
   * @private
   * @param {string} collectionId
   * @return {!Promise<?Array<
   *     !chromeos.personalizationApp.mojom.WallpaperImage>>}
   */
  async fetchImages_(collectionId) {
    assert(
        (typeof collectionId === 'string') && collectionId.length > 0,
        'Collection id parameter is required');

    if (this.cache_.has(collectionId)) {
      return this.cache_.get(collectionId);
    }

    try {
      const {images} = await fetchImagesForCollectionHelper(
          this.wallpaperProvider_, collectionId);
      this.cache_.set(collectionId, images);
      return images;
    } catch (e) {
      // TODO(b/181697575) handle errors and allow user to retry
      console.warn(
          'Fetching wallpaper collection images failed for collection id',
          collectionId);
      return null;
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
    return !loading && isNonEmptyArray(images);
  }

  /**
   * Receives events from untrusted iframe. Expects only SelectImageEvent type.
   * @private
   * @param {!Event} event
   */
  async onImageSelected_(event) {
    /** @type {!chromeos.personalizationApp.mojom.WallpaperImage} */
    const image =
        validateReceivedSelection(event, EventType.SELECT_IMAGE, this.images_);

    // TODO(b/178215472) show a loading indicator.
    const {success} =
        await this.wallpaperProvider_.selectWallpaper(image.assetId);

    // TODO(b/181697575) show a user facing error and handle failure cases.
    if (!success) {
      console.warn('Setting wallpaper image failed');
    }
  }
}

customElements.define(WallpaperImages.is, WallpaperImages);

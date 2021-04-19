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
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {EventType} from '../common/constants.js';
import {sendImages, validateReceivedSelection} from '../common/iframe_api.js';
import {isNonEmptyArray} from '../common/utils.js';
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
        observer: 'collectionIdChanged_',
      },

      /**
       * Used to bind/unbind the message listener when this element is shown or
       * hidden. Also clears out the untrusted iframe when active is false.
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
    this.onIframeLoaded_ = this.onIframeLoaded_.bind(this);
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

  onActiveChanged_(value) {
    const func = value ? window.addEventListener : window.removeEventListener;
    func('message', this.onImageSelected_);
    this.hidden = !value;

    // Reset images to empty in the untrusted iframe. Prevents a flash of old
    // content when switching between collections.
    const iframe = this.shadowRoot.getElementById('images-iframe');
    if (this.hidden && iframe && iframe.contentWindow) {
      sendImagesFunction(iframe.contentWindow, []);
    }
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
    return !loading && isNonEmptyArray(images);
  }

  /**
   * Called when the iframe is loaded. Guaranteed to be after the initial images
   * loading has completed.
   * @private
   */
  onIframeLoaded_() {
    const iframe = this.shadowRoot.getElementById('images-iframe');
    sendImagesFunction(iframe.contentWindow, this.images_);
  }

  /**
   * Receives events from untrusted iframe. Expects only SelectImageEvent type.
   * @private
   * @param {!Event} event
   */
  onImageSelected_(event) {
    /** @type {!chromeos.personalizationApp.mojom.WallpaperImage} */
    const image =
        validateReceivedSelection(event, EventType.SELECT_IMAGE, this.images_);

    // TODO(b/178017996) set image as wallpaper when clicked.
    console.warn(
        'onImageSelected not implemented yet. Selected', image.url.url);
  }
}

customElements.define(WallpaperImages.is, WallpaperImages);

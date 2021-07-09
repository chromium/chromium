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
import '../common/styles.js';
import {assert} from '/assert.m.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {isNonEmptyArray, stringToUnguessableToken, unguessableTokensEqual, unguessableTokenToString} from '../common/utils.js';
import {getWallpaperProvider} from './mojo_interface_provider.js';
import {selectWallpaper} from './personalization_controller.js';
import {WithPersonalizationStore} from './personalization_store.js';

/** @polymer */
export class LocalImages extends WithPersonalizationStore {
  static get is() {
    return 'local-images';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      hidden: {
        type: Boolean,
        value: true,
        reflectToAttribute: true,
      },

      /**
       * @type {!Array<!chromeos.personalizationApp.mojom.LocalImage>}
       * @private
       */
      images_: {
        type: Array,
      },

      /** @private */
      imagesLoading_: {
        type: Boolean,
      },

      /**
       * Mapping of stringified local image id to data url.
       * @type {!Object<string, string>}
       * @private
       */
      imageData_: {
        type: Object,
      },

      /**
       * Mapping of stringified local image id to boolean.
       * @type {!Object<string, boolean>}
       * @private
       */
      imageDataLoading_: {
        type: Object,
      },

      /** @private */
      hasError_: {
        type: Boolean,
        computed: 'computeHasError_(imagesLoading_, images_)',
      },

      /** @private */
      showImages_: {
        type: Boolean,
        computed: 'computeShowImages_(imagesLoading_, images_)'
      }
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.watch('images_', state => state.local.images);
    this.watch('imagesLoading_', state => state.loading.local.images);
    this.watch('imageData_', state => state.local.data);
    this.watch('imageDataLoading_', state => state.loading.local.data);
    this.updateFromStore();
  }

  /**
   * @private
   * @param {boolean} imagesLoading
   * @param {?Array<!chromeos.personalizationApp.mojom.LocalImage>} images
   * @return {boolean}
   */
  computeHasError_(imagesLoading, images) {
    return !imagesLoading && !isNonEmptyArray(images);
  }

  /**
   * @private
   * @param {boolean} imagesLoading
   * @param {?Array<!chromeos.personalizationApp.mojom.LocalImage>} images
   * @return {boolean}
   */
  computeShowImages_(imagesLoading, images) {
    return !imagesLoading && isNonEmptyArray(images);
  }

  /**
   * Forces iron-list to re-evaluate when hidden changes.
   * @private
   * @param {boolean} hidden
   * @param {!Array<!chromeos.personalizationApp.mojom.LocalImage>} images
   * @return {!Array<!chromeos.personalizationApp.mojom.LocalImage>}
   */
  getImages_(hidden, images) {
    return hidden ? [] : images;
  }

  /**
   * @param {boolean} hidden
   * @param {!Array<!chromeos.personalizationApp.mojom.LocalImage>} images
   * @return {number}
   */
  getImageCount_(hidden, images) {
    return this.getImages_(hidden, images).length;
  }

  /**
   * TODO(b/192975897) compare with currently selected image to return correct
   * aria-selected attribute.
   * @param {!chromeos.personalizationApp.mojom.LocalImage} image
   * @return {string}
   */
  getAriaSelected_(image) {
    return 'false';
  }

  /**
   * @private
   * @param {chromeos.personalizationApp.mojom.LocalImage} image
   * @param {Object<string, string>} imageData
   * @param {Object<string, boolean>} imageDataLoading
   * @return {boolean}
   */
  shouldShowImage_(image, imageData, imageDataLoading) {
    if (!image || !imageData || !imageDataLoading) {
      return false;
    }
    const key = unguessableTokenToString(image.id);
    return !!imageData[key] && imageDataLoading[key] === false;
  }

  /**
   * @private
   * @param {chromeos.personalizationApp.mojom.LocalImage} image
   * @param {Object<string, string>} imageData
   * @return {string}
   */
  getImageData_(image, imageData) {
    const key = unguessableTokenToString(image.id);
    return imageData[key];
  }

  /**
   * @private
   * @param {!chromeos.personalizationApp.mojom.LocalImage} image
   * @return {string}
   */
  getImageKey_(image) {
    return unguessableTokenToString(image.id);
  }

  /**
   * @private
   * @param {!Event} event
   */
  onClickImage_(event) {
    const id = stringToUnguessableToken(event.currentTarget.dataset.id);
    const image =
        this.images_.find(image => unguessableTokensEqual(id, image.id));
    assert(!!image, 'Image with that id not found');
    selectWallpaper(
        /** @type {!chromeos.personalizationApp.mojom.LocalImage} */ (image),
        getWallpaperProvider(), this.getStore());
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

customElements.define(LocalImages.is, LocalImages);

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
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import './styles.js';
import '../common/icons.js';
import '../common/styles.js';
import {assert} from '/assert.m.js';
import {afterNextRender, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getLoadingPlaceholderAnimationDelay} from '../common/utils.js';
import {isSelectionEvent, stringToUnguessableToken, unguessableTokensEqual, unguessableTokenToString} from '../common/utils.js';
import {getWallpaperProvider} from './mojo_interface_provider.js';
import {selectWallpaper} from './personalization_controller.js';
import {DisplayableImage} from './personalization_reducers.js';
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
        observer: 'onHiddenChanged_',
      },

      /**
       * @type {!Array<!chromeos.personalizationApp.mojom.LocalImage>}
       * @private
       */
      images_: {
        type: Array,
        observer: 'onImagesChanged_',
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

      /**
       * @type {?chromeos.personalizationApp.mojom.CurrentWallpaper}
       * @private
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

      /**
       * @type {!Array<!chromeos.personalizationApp.mojom.LocalImage>}
       * @private
       */
      imagesToDisplay_: {
        type: Array,
        value: [],
      }
    };
  }

  static get observers() {
    return ['onImageLoaded_(imageData_, imageDataLoading_)']
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.watch('images_', state => state.local.images);
    this.watch('imageData_', state => state.local.data);
    this.watch('imageDataLoading_', state => state.loading.local.data);
    this.watch('currentSelected_', state => state.currentSelected);
    this.watch('pendingSelected_', state => state.pendingSelected);
    this.updateFromStore();
  }

  /**
   * When iron-list items change while parent element is hidden, iron-list will
   * render incorrectly. Force another layout to happen by calling iron-resize
   * when this element is visible again.
   * @param {boolean} hidden
   * @private
   */
  onHiddenChanged_(hidden) {
    if (!hidden) {
      document.title = this.i18n('myImagesLabel');
      this.shadowRoot.getElementById('main').focus();
      afterNextRender(this, () => {
        this.shadowRoot.querySelector('iron-list').fire('iron-resize');
      });
    }
  }

  /**
   * Sets |imagesToDisplay| when a new set of local images loads.
   * @param {Array<!chromeos.personalizationApp.mojom.LocalImage>} images
   * @private
   */
  onImagesChanged_(images) {
    this.imagesToDisplay_ = (images || []).filter(image => {
      const key = unguessableTokenToString(image);
      if (this.imageDataLoading_[key] === false) {
        return !!this.imageData_[key];
      }
      return true;
    });
  }

  /**
   * Called each time a new image thumbnail is loaded. Removes images
   * from the list of displayed images if it has failed to load.
   * @param {Object<string, string>} imageData
   * @param {Object<string, boolean>} imageDataLoading
   * @private
   */
  onImageLoaded_(imageData, imageDataLoading) {
    if (!imageData || !imageDataLoading) {
      return;
    }
    // Iterate backwards in case we need to splice to remove from
    // |imagesToDisplay| while iterating.
    for (let i = this.imagesToDisplay_.length - 1; i >= 0; i--) {
      const image = this.imagesToDisplay_[i];
      const key = unguessableTokenToString(image.id);
      const failed = imageDataLoading[key] === false && !imageData[key];
      if (failed) {
        this.splice('imagesToDisplay_', i, 1);
      }
    }
  }


  /**
   * @param {!chromeos.personalizationApp.mojom.LocalImage} image
   * @param {?chromeos.personalizationApp.mojom.CurrentWallpaper}
   *     currentSelected
   * @param {?DisplayableImage} pendingSelected
   * @return {string}
   * @private
   */
  getAriaSelected_(image, currentSelected, pendingSelected) {
    if (!image || (!currentSelected && !pendingSelected)) {
      return 'false';
    }
    return (!!pendingSelected && image.id === pendingSelected.id ||
            !!currentSelected && currentSelected.key === image.name &&
                !pendingSelected)
        .toString();
  }

  /**
   * @param {chromeos.personalizationApp.mojom.LocalImage} image
   * @param {Object<string, boolean>} imageDataLoading
   * @return {boolean}
   * @private
   */
  isImageLoading_(image, imageDataLoading) {
    if (!image || !imageDataLoading) {
      return true;
    }
    const key = unguessableTokenToString(image.id);
    // If key is not present, then loading has not yet started. Still show a
    // loading tile in this case.
    return !imageDataLoading.hasOwnProperty(key) ||
        imageDataLoading[key] === true;
  }

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  getLoadingPlaceholderAnimationDelay_(index) {
    return getLoadingPlaceholderAnimationDelay(index);
  }

  /**
   * @param {chromeos.personalizationApp.mojom.LocalImage} image
   * @param {Object<string, string>} imageData
   * @param {Object<string, boolean>} imageDataLoading
   * @return {boolean}
   * @private
   */
  isImageReady_(image, imageData, imageDataLoading) {
    if (!image || !imageData || !imageDataLoading) {
      return false;
    }
    const key = unguessableTokenToString(image.id);
    return !!imageData[key] && imageDataLoading[key] === false;
  }

  /**
   * @param {chromeos.personalizationApp.mojom.LocalImage} image
   * @param {Object<string, string>} imageData
   * @return {string}
   * @private
   */
  getImageData_(image, imageData) {
    const key = unguessableTokenToString(image.id);
    return imageData[key];
  }

  /**
   * @param {!chromeos.personalizationApp.mojom.LocalImage} image
   * @return {string}
   * @private
   */
  getImageKey_(image) {
    return unguessableTokenToString(image.id);
  }

  /**
   * @param {!Event} event
   * @private
   */
  onImageSelected_(event) {
    if (!isSelectionEvent(event)) {
      return;
    }
    const id = stringToUnguessableToken(event.currentTarget.dataset.id);
    const image =
        this.images_.find(image => unguessableTokensEqual(id, image.id));
    assert(!!image, 'Image with that id not found');
    selectWallpaper(
        /** @type {!chromeos.personalizationApp.mojom.LocalImage} */ (image),
        getWallpaperProvider(), this.getStore());
  }

  /**
   * @param {number} i
   * @return {number}
   * @private
   */
  getAriaIndex_(i) {
    return i + 1;
  }
}

customElements.define(LocalImages.is, LocalImages);

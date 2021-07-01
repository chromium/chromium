// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays the currently selected
 * wallpaper.
 */

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getWallpaperProvider} from './mojo_interface_provider.js';
import {getCurrentWallpaper} from './personalization_controller.js';
import {WithPersonalizationStore} from './personalization_store.js';

/**
 * Wallpaper images sometimes have a resolution suffix appended to the end of
 * the image. This is typically to fetch a high resolution image to show as the
 * user's wallpaper. We do not want the full resolution here, so remove the
 * suffix to get a 512x512 preview.
 * TODO(b/186807814) support different resolution parameters here.
 * @param {string} url
 * @return {string}
 */
function removeHighResolutionSuffix(url) {
  return url.replace(/=w\d+$/, '');
}

/**
 * Returns whether the given URL starts with http:// or https://.
 * @param {string} url URL to check.
 */
function hasHttpScheme(url) {
  return url.startsWith('http://') || url.startsWith('https://');
}

/** @polymer */
export class WallpaperSelected extends WithPersonalizationStore {
  static get is() {
    return 'wallpaper-selected';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {?chromeos.personalizationApp.mojom.WallpaperImage}
       * @private
       */
      image_: {
        type: Object,
      },

      /** @private */
      isLoading_: {
        type: Boolean,
        listener: 'onIsLoadingChanged_',
      },

      /** @private */
      hasError_: {
        type: Boolean,
        computed: 'computeHasError_(image_, isLoading_)',
      },

      /** @private */
      showImage_: {
        type: Boolean,
        computed: 'computeShowImage_(image_, isLoading_)',
      },
    };
  }

  constructor() {
    super();
    /** @private */
    this.wallpaperProvider_ = getWallpaperProvider();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.watch('image_', state => state.selected);
    this.watch('isLoading_', state => state.loading.selected);
    this.updateFromStore();
    getCurrentWallpaper(this.wallpaperProvider_, this.getStore());
  }

  /**
   * Return a chrome://image or data:// url to load the image safely. Returns
   * empty string in case |image| is null or invalid.
   * @param {?chromeos.personalizationApp.mojom.CurrentWallpaper} image
   * @return {string}
   * @private
   */
  getImageSrc_(image) {
    if (image && image.url) {
      if (hasHttpScheme(image.url.url))
        return `chrome://image?${removeHighResolutionSuffix(image.url.url)}`;
      return image.url.url;
    }
    return '';
  }

  /**
   * @private
   * @param {?chromeos.personalizationApp.mojom.WallpaperImage} image
   * @param {boolean} loading
   * @return {boolean}
   */
  computeShowImage_(image, loading) {
    return !loading && !!image;
  }

  /**
   * @private
   * @param {?chromeos.personalizationApp.mojom.WallpaperImage} image
   * @param {boolean} loading
   * @return {boolean}
   */
  computeHasError_(image, loading) {
    return !loading && !image;
  }

  /**
   * @private
   * @param {?chromeos.personalizationApp.mojom.WallpaperImage} image
   * @return {string}
   */
  getAriaLabel_(image) {
    // TODO(b/192195088) figure out aria label when image has no attribution
    return [this.i18n('currentlySet'), ...(image?.attribution || [])].join(' ');
  }
}

customElements.define(WallpaperSelected.is, WallpaperSelected);

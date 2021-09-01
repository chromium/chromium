// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays the currently selected
 * wallpaper.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import '../common/icons.js';
import {assert} from 'chrome://resources/js/assert.m.js'
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {isNonEmptyArray} from '../common/utils.js';
import {getWallpaperProvider} from './mojo_interface_provider.js';
import {beginLoadSelectedImageAction, setSelectedImageAction} from './personalization_actions.js';
import {getDailyRefreshCollectionId, setCustomWallpaperLayout, setDailyRefreshCollectionId, updateDailyRefreshWallpaper} from './personalization_controller.js';
import {WallpaperLayout, WallpaperType} from './personalization_reducers.js';
import {Paths} from './personalization_router_element.js';
import {WithPersonalizationStore} from './personalization_store.js';

/**
 * Set up the observer to listen for wallpaper changes.
 * @param {!chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 *     wallpaperProvider
 * @param {!chromeos.personalizationApp.mojom.WallpaperObserverInterface} target
 * @return {!chromeos.personalizationApp.mojom.WallpaperObserverReceiver}
 */
function initWallpaperObserver(wallpaperProvider, target) {
  const receiver =
      new chromeos.personalizationApp.mojom.WallpaperObserverReceiver(target);
  wallpaperProvider.setWallpaperObserver(receiver.$.bindNewPipeAndPassRemote());
  return receiver;
}

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

/**
 * @polymer
 * @implements {chromeos.personalizationApp.mojom.WallpaperObserverInterface}
 */
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
       * The current collection id to display.
       */
      collectionId: {
        type: String,
      },

      /**
       * The current path of the page.
       */
      path: {
        type: String,
      },

      /**
       * @type {?chromeos.personalizationApp.mojom.CurrentWallpaper}
       * @private
       */
      image_: {
        type: Object,
        observer: 'onImageChanged_',
      },

      /**
       * @type {!string}
       * @private
       */
      imageTitle_: {
        type: String,
        computed: 'computeImageTitle_(image_, dailyRefreshCollectionId_)',
      },

      /**
       * @type {Array<!string>}
       * @private
       */
      imageOtherAttribution_: {
        type: Array,
        computed: 'computeImageOtherAttribution_(image_)',
      },

      /**
       * @type {?string}
       * @private
       */
      dailyRefreshCollectionId_: {
        type: String,
      },

      /** @private */
      isLoading_: {
        type: Boolean,
      },

      /** @private */
      hasError_: {
        type: Boolean,
        computed: 'computeHasError_(image_, isLoading_, error_)',
      },

      /** @private */
      showImage_: {
        type: Boolean,
        computed: 'computeShowImage_(image_, isLoading_)',
      },

      /** @private */
      showWallpaperOptions_: {
        type: Boolean,
        computed: 'computeShowWallpaperOptions_(image_, path)',
      },

      /** @private */
      showCollectionOptions_: {
        type: Boolean,
        computed: 'computeShowCollectionOptions_(path)',
      },

      /** @private */
      showRefreshButton_: {
        type: Boolean,
        computed:
            'isDailyRefreshCollectionId_(collectionId,dailyRefreshCollectionId_)',
      },

      /** @private */
      dailyRefreshIcon_: {
        type: String,
        computed:
            'computeDailyRefreshIcon_(collectionId,dailyRefreshCollectionId_)',
      },

      /** @private */
      ariaPressed_: {
        type: String,
        computed: 'computeAriaPressed_(collectionId,dailyRefreshCollectionId_)',
      },

      /** @private */
      fillIcon_: {
        type: String,
        computed: 'computeFillIcon_(image_)',
      },

      /** @private */
      centerIcon_: {
        type: String,
        computed: 'computeCenterIcon_(image_)',
      },

      /**
       * @private
       */
      error_: {
        type: String,
        value: null,
      },
    };
  }

  constructor() {
    super();
    /** @private */
    this.wallpaperProvider_ = getWallpaperProvider();
    /** @private */
    this.wallpaperObserver_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.dispatch(beginLoadSelectedImageAction());
    this.wallpaperObserver_ =
        initWallpaperObserver(this.wallpaperProvider_, this);
    this.watch('error_', state => state.error);
    this.watch('image_', state => state.currentSelected);
    this.watch(
        'isLoading_',
        state => state.loading.setImage > 0 || state.loading.selected ||
            state.loading.refreshWallpaper);
    this.watch(
        'dailyRefreshCollectionId_', state => state.dailyRefresh.collectionId);
    this.updateFromStore();
    getDailyRefreshCollectionId(this.wallpaperProvider_, this.getStore());
  }

  /** @override */
  disconnectedCallback() {
    this.wallpaperObserver_.$.close();
  }

  /**
   * Called when the wallpaper changes.
   * @param {?chromeos.personalizationApp.mojom.CurrentWallpaper}
   *     currentWallpaper
   */
  onWallpaperChanged(currentWallpaper) {
    this.dispatch(setSelectedImageAction(currentWallpaper));
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
   * @param {?chromeos.personalizationApp.mojom.CurrentWallpaper} image
   * @param {boolean} loading
   * @return {boolean}
   * @private
   */
  computeShowImage_(image, loading) {
    // Specifically check === false to avoid undefined case while component is
    // initializing.
    return loading === false && !!image;
  }

  /**
   * @param {?chromeos.personalizationApp.mojom.CurrentWallpaper} image
   * @param {!string} dailyRefreshCollectionId
   * @return {string}
   * @private
   */
  computeImageTitle_(image, dailyRefreshCollectionId) {
    if (!image)
      return this.i18n('unknownImageAttribution');
    if (isNonEmptyArray(image.attribution)) {
      let title = image.attribution[0];
      return !!dailyRefreshCollectionId ?
          this.i18n('dailyRefresh') + ': ' + title :
          title;
    } else {
      // Fallback to cached attribution.
      let attribution = this.getLocalStorageAttribution(image.key);
      if (isNonEmptyArray(attribution)) {
        let title = attribution[0];
        return !!dailyRefreshCollectionId ?
            this.i18n('dailyRefresh') + ': ' + title :
            title;
      }
    }
    return this.i18n('unknownImageAttribution');
  }

  /**
   * @param {?chromeos.personalizationApp.mojom.CurrentWallpaper} image
   * @return {Array<!string>}
   * @private
   */
  computeImageOtherAttribution_(image) {
    if (!image)
      return [];
    if (isNonEmptyArray(image.attribution))
      return image.attribution.slice(1);
    // Fallback to cached attribution.
    let attribution = this.getLocalStorageAttribution(image.key);
    if (isNonEmptyArray(attribution)) {
      return attribution.slice(1);
    }
    return [];
  }

  /**
   * @param {?chromeos.personalizationApp.mojom.CurrentWallpaper} image
   * @param {string} path
   * @return {boolean}
   * @private
   */
  computeShowWallpaperOptions_(image, path) {
    return !!image && image.type === WallpaperType.kCustomized &&
        path === Paths.LocalCollection;
  }

  /**
   * @param {string} path
   * @return {boolean}
   * @private
   */
  computeShowCollectionOptions_(path) {
    return path === Paths.CollectionImages;
  }

  /**
   * @param {!chromeos.personalizationApp.mojom.CurrentWallpaper} image
   * @return {string}
   * @private
   */
  getCenterAriaSelected_(image) {
    return (!!image && image.layout === WallpaperLayout.kCenter).toString();
  }

  /**
   * @param {!chromeos.personalizationApp.mojom.CurrentWallpaper} image
   * @return {string}
   * @private
   */
  getFillAriaSelected_(image) {
    return (!!image && image.layout === WallpaperLayout.kCenterCropped)
        .toString();
  }

  /**
   * @param {!chromeos.personalizationApp.mojom.CurrentWallpaper} image
   * @return {string}
   * @private
   */
  computeFillIcon_(image) {
    if (!!image && image.layout === WallpaperLayout.kCenterCropped)
      return 'personalization:checkmark';
    return 'personalization:layout_fill';
  }

  /**
   * @param {!chromeos.personalizationApp.mojom.CurrentWallpaper} image
   * @return {string}
   * @private
   */
  computeCenterIcon_(image) {
    if (!!image && image.layout === WallpaperLayout.kCenter)
      return 'personalization:checkmark';
    return 'personalization:layout_center';
  }

  /**
   * @param {!Event} event
   * @private
   */
  onClickLayoutIcon_(event) {
    const image = this.getState().currentSelected;
    const layout =
        this.getWallpaperLayoutEnum(event.currentTarget.dataset.layout);
    if (image.layout === layout)
      return;
    assert(
        layout === WallpaperLayout.kCenter ||
        layout === WallpaperLayout.kCenterCropped);
    setCustomWallpaperLayout(layout, this.wallpaperProvider_, this.getStore());
  }

  /**
   * @param {!string} collectionId
   * @param {!string} dailyRefreshCollectionId
   * @return {string}
   * @private
   */
  computeDailyRefreshIcon_(collectionId, dailyRefreshCollectionId) {
    if (this.isDailyRefreshCollectionId_(
            collectionId, dailyRefreshCollectionId))
      return 'personalization:checkmark';
    return 'personalization:change-daily';
  }

  /**
   * @param {!string} collectionId
   * @param {!string} dailyRefreshCollectionId
   * @return {string}
   * @private
   */
  computeAriaPressed_(collectionId, dailyRefreshCollectionId) {
    if (this.isDailyRefreshCollectionId_(
            collectionId, dailyRefreshCollectionId))
      return 'true';
    return 'false';
  }

  /**
   * @private
   * @param {!Event} event
   */
  onClickDailyRefreshToggle_(event) {
    const collectionId = event.currentTarget.dataset['collectionId'];
    const dailyRefreshCollectionId =
        event.currentTarget.dataset['dailyRefreshCollectionId'];
    const isDailyRefreshCollectionId = this.isDailyRefreshCollectionId_(
        collectionId, dailyRefreshCollectionId);
    setDailyRefreshCollectionId(
        isDailyRefreshCollectionId ? '' : collectionId, this.wallpaperProvider_,
        this.getStore());
    // Only refresh the wallpaper if daily refresh is toggled on.
    if (!isDailyRefreshCollectionId) {
      updateDailyRefreshWallpaper(this.wallpaperProvider_, this.getStore());
    }
  }

  /**
   * Determine the current collection view belongs to the collection that is
   * enabled with daily refresh. If true, highlight the toggle and display the
   * refresh button
   * @param {!string} collectionId
   * @param {!string} dailyRefreshCollectionId
   * @return {boolean}
   * @private
   */
  isDailyRefreshCollectionId_(collectionId, dailyRefreshCollectionId) {
    return collectionId === dailyRefreshCollectionId;
  }

  /**
   * @private
   */
  onClickUpdateDailyRefreshWallpaper_() {
    updateDailyRefreshWallpaper(this.wallpaperProvider_, this.getStore());
  }

  /**
   * Determine whether there is an error in showing selected image. An error
   * happens when there is no previously loaded image and either no new image
   * is being loaded or there is an error from upstream.
   * @param {?chromeos.personalizationApp.mojom.CurrentWallpaper} image
   * @param {boolean} loading
   * @param {?string} error
   * @return {boolean}
   * @private
   */
  computeHasError_(image, loading, error) {
    return (!loading || !!error) && !image;
  }

  /**
   * @param {?chromeos.personalizationApp.mojom.CurrentWallpaper} image
   * @return {string}
   * @private
   */
  getAriaLabel_(image) {
    if (!image) {
      return this.i18n('currentlySet') + ' ' +
          this.i18n('unknownImageAttribution');
    }
    if (isNonEmptyArray(image.attribution)) {
      return [this.i18n('currentlySet'), ...image.attribution].join(' ');
    }
    // Fallback to cached attribution.
    let attribution =
        /** @type {!Iterable} */ (this.getLocalStorageAttribution(image.key));
    if (isNonEmptyArray(attribution)) {
      return [this.i18n('currentlySet'), ...attribution].join(' ');
    }
    return this.i18n('currentlySet') + ' ' +
        this.i18n('unknownImageAttribution');
  }

  /**
   * Returns hidden state of loading placeholder.
   * @param {boolean} loading
   * @param {boolean} showImage
   * @return {boolean}
   * @private
   */
  showPlaceholders_(loading, showImage) {
    return loading || !showImage;
  }

  /**
   * Cache the attribution in local storage when image is updated
   * Populate the attribution map in local storage when image is updated
   * @param {?chromeos.personalizationApp.mojom.CurrentWallpaper} newImage
   * @param {?chromeos.personalizationApp.mojom.CurrentWallpaper} oldImage
   * @private
   */
  async onImageChanged_(newImage, oldImage) {
    let attributionMap = /** @type {Object<string, Array<string>>} */ (
        JSON.parse((window.localStorage['attribution'] || '{}')));
    if (attributionMap.size == 0 ||
        !!newImage && !!oldImage && newImage.key !== oldImage.key) {
      attributionMap[newImage.key] = newImage.attribution;
      delete attributionMap[oldImage.key];
      window.localStorage['attribution'] = JSON.stringify(attributionMap);
    }
  }

  /**
   * @param {string} key
   * @return {Array<!string>}
   */
  getLocalStorageAttribution(key) {
    let attributionMap = /** @type {Object<string, Array<string>>} */ (
        JSON.parse((window.localStorage['attribution'] || '{}')));
    let attribution = attributionMap[key];
    if (!attribution) {
      console.warn('Unable to get attribution from local storage.', key);
    }
    return attribution;
  }

  /**
   * @param {string} layout
   * @return {chromeos.personalizationApp.mojom.WallpaperLayout}
   */
  getWallpaperLayoutEnum(layout) {
    switch (layout) {
      case 'CENTER':
        return WallpaperLayout.kCenter;
      case 'FILL':
        return WallpaperLayout.kCenterCropped;
      default:
        return WallpaperLayout.kCenter;
    }
  }
}

customElements.define(WallpaperSelected.is, WallpaperSelected);

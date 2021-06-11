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
import {sendCollections} from '../common/iframe_api.js';
import {isNonEmptyArray, promisifyOnload, unguessableTokenToString} from '../common/utils.js';
import {getWallpaperProvider} from './mojo_interface_provider.js';
import {getAllLocalImageThumbnails, getLocalImages, initializeBackdropData} from './personalization_controller.js';
import {WithPersonalizationStore} from './personalization_store.js';

let sendCollectionsFunction = sendCollections;

export function promisifySendCollectionsForTesting() {
  let resolver;
  const promise = new Promise((resolve) => resolver = resolve);
  sendCollectionsFunction = (...args) => resolver(args);
  return promise;
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
       * @private
       * @type {!Array<string>}
       */
      localImages_: {
        type: Array,
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

  constructor() {
    super();
    /** @private */
    this.wallpaperProvider_ = getWallpaperProvider();
    this.iframePromise_ = /** @type {!Promise<!HTMLIFrameElement>} */ (
        promisifyOnload(this, 'collections-iframe', afterNextRender));
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.watch('collections_', state => state.backdrop.collections);
    this.watch('collectionsLoading_', state => state.loading.collections);
    this.watch(
        'localImages_',
        state => Array.isArray(state.local.images) ?
            state.local.images.map(image => unguessableTokenToString(image.id))
                .filter(id => !!state.local.data[id])
                .map(id => state.local.data[id]) :
            null);
    this.updateFromStore();
    const store = this.getStore();
    initializeBackdropData(this.wallpaperProvider_, store);
    getLocalImages(this.wallpaperProvider_, store)
        .then(() => getAllLocalImageThumbnails(this.wallpaperProvider_, store));
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
}

customElements.define(WallpaperCollections.is, WallpaperCollections);

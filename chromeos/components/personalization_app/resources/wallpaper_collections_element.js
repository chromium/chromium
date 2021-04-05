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
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {fetchCollectionsHelper, getWallpaperProvider} from './mojo_interface_provider.js';

export class WallpaperCollections extends PolymerElement {
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
        computed: 'computeHasError_(collections_, isLoading_)',
      },

      /** @private */
      showCollections_: {
        type: Boolean,
        computed: 'computeShowCollections_(collections_, isLoading_)',
      },

    };
  }

  constructor() {
    super();
    /** @private */
    this.wallpaperProvider_ = getWallpaperProvider();
  }

  ready() {
    super.ready();
    this.fetchCollections_();
  }

  /** @private */
  async fetchCollections_() {
    this.setProperties({isLoading_: true, collections_: null});
    try {
      const {collections} =
          await fetchCollectionsHelper(this.wallpaperProvider_);
      this.collections_ = collections;
    } catch (e) {
      console.warn('Fetching wallpaper collections failed', e);
    } finally {
      this.isLoading_ = false;
    }
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
    return !loading && Array.isArray(collections) && collections.length > 0;
  }

  /**
   * @private
   * @param {!chromeos.personalizationApp.mojom.WallpaperCollection} collection
   * @return {string} The link to navigate to this collection.
   */
  collectionHref_(collection) {
    return `/collection?id=${collection.id}`;
  }
}

customElements.define(WallpaperCollections.is, WallpaperCollections);

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
import {afterNextRender, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {EventType} from '../common/constants.js';
import {sendCollections, validateReceivedSelection} from '../common/iframe_api.js';
import {isNonEmptyArray, promisifyOnload} from '../common/utils.js';
import {fetchCollectionsHelper, getWallpaperProvider} from './mojo_interface_provider.js';

let sendCollectionsFunction = sendCollections;

export function promisifySendCollectionsForTesting() {
  let resolver;
  const promise = new Promise((resolve) => resolver = resolve);
  sendCollectionsFunction = (...args) => resolver(args);
  return promise;
}

export class WallpaperCollections extends PolymerElement {
  static get is() {
    return 'wallpaper-collections';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {function(!string)} */
      selectCollection: {
        type: Function,
      },

      /**
       * Used to bind/unbind the message listener when this element is shown or
       * hidden.
       */
      active: {
        type: Boolean,
        observer: 'onActiveChanged_',
      },

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
    this.iframePromise_ = /** @type {!Promise<!HTMLIFrameElement>} */ (
        promisifyOnload(this, 'collections-iframe', afterNextRender));
    this.onCollectionSelected_ = this.onCollectionSelected_.bind(this);
    /** @private */
    this.wallpaperProvider_ = getWallpaperProvider();
  }

  /** @override */
  ready() {
    super.ready();
    this.fetchCollections_();
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('message', this.onCollectionSelected_);
  }

  /**
   * @private
   * @param {boolean} value
   */
  onActiveChanged_(value) {
    const func = value ? window.addEventListener : window.removeEventListener;
    func('message', this.onCollectionSelected_);
    this.hidden = !value;
  }

  /** @private */
  async fetchCollections_() {
    this.setProperties({isLoading_: true, collections_: null});
    // Make sure iframe is fully loaded.
    const iframe = await this.iframePromise_;
    try {
      const {collections} =
          await fetchCollectionsHelper(this.wallpaperProvider_);
      sendCollectionsFunction(iframe.contentWindow, collections);
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
    return !loading && isNonEmptyArray(collections);
  }

  /**
   * Called when untrusted iframe sends a selection back.
   * @private
   * @param {!Event} event
   */
  onCollectionSelected_(event) {
    /** @type {!chromeos.personalizationApp.mojom.WallpaperCollection} */
    const collection = validateReceivedSelection(
        event, EventType.SELECT_COLLECTION, this.collections_);
    this.selectCollection(collection.id);
  }
}

customElements.define(WallpaperCollections.is, WallpaperCollections);

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//personalization/polymer/v3_0/iron-list/iron-list.js';
import './styles.js';
import {html, PolymerElement} from 'chrome-untrusted://personalization/polymer/v3_0/polymer/polymer_bundled.min.js';
import {EventType} from '../common/constants.js';
import {selectCollection, validateReceivedData} from '../common/iframe_api.js';

/**
 * @fileoverview Responds to |SendCollectionsEvent| from trusted. Handles user
 * input and responds with |SelectCollectionEvent| when an image is selected.
 */

class CollectionsGrid extends PolymerElement {
  static get is() {
    return 'collections-grid';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {!Array<!chromeos.personalizationApp.mojom.WallpaperCollection>}
       * @private
       */
      collections_: {
        type: Array,
        value: [],
      },
    };
  }

  constructor() {
    super();
    this.onMessageReceived_ = this.onMessageReceived_.bind(this);
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    window.addEventListener('message', this.onMessageReceived_);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('message', this.onMessageReceived_);
  }

  /**
   * Handler for messages from trusted code. Expects only SendImagesEvent and
   * will error on any other event.
   * @param {!Event} message
   * @private
   */
  onMessageReceived_(message) {
    try {
      this.collections_ =
          validateReceivedData(message, EventType.SEND_COLLECTIONS);
    } catch (e) {
      console.warn('Invalid collections received', e);
      this.collections_ = [];
    }
  }

  /**
   * Notify trusted code that a user clicked on a collection.
   * @private
   * @param {!Event} e
   */
  onCollectionClicked_(e) {
    selectCollection(window.parent, e.currentTarget.dataset.id);
  }
}

customElements.define(CollectionsGrid.is, CollectionsGrid);

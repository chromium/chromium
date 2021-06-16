// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://personalization/polymer/v3_0/iron-list/iron-list.js';
import '../common/styles.js';
import {html, PolymerElement} from 'chrome-untrusted://personalization/polymer/v3_0/polymer/polymer_bundled.min.js';
import {EventType} from '../common/constants.js';
import {selectImage, validateReceivedData} from '../common/iframe_api.js';

/**
 * @fileoverview Responds to |SendImagesEvent| from trusted. Handles user input
 * and responds with |SelectImageEvent| when an image is selected.
 */

class ImagesGrid extends PolymerElement {
  static get is() {
    return 'images-grid';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {!Array<!chromeos.personalizationApp.mojom.WallpaperImage>}
       * @private
       */
      images_: {
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
      this.images_ = validateReceivedData(message, EventType.SEND_IMAGES);
    } catch (e) {
      console.warn('Invalid images received', e);
      this.images_ = [];
    }
  }

  /**
   * Notify trusted code that a user clicked on an image.
   * @private
   * @param {!Event} e
   */
  onImageClicked_(e) {
    const img = e.currentTarget;
    selectImage(window.parent, BigInt(img.dataset.id));
  }
}

customElements.define(ImagesGrid.is, ImagesGrid);

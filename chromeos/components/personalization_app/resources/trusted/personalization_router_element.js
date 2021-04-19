// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This router component hooks into the current url path and query
 * parameters to display sections of the personalization SWA.
 */

import 'chrome://resources/polymer/v3_0/iron-location/iron-location.js';
import 'chrome://resources/polymer/v3_0/iron-location/iron-query-params.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @enum {string} */
const Paths = {
  CollectionImages: '/collection',
  Collections: '/',
};

export class PersonalizationRouter extends PolymerElement {
  static get is() {
    return 'personalization-router';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      path_: {
        type: String,
      },

      /** @private */
      query_: {
        type: String,
      },

      /** @private */
      queryParams_: {
        type: Object,
      },
    };
  }

  constructor() {
    super();
    this.selectCollection_ = this.selectCollection_.bind(this);
  }

  /**
   * @param {string} path
   * @return {boolean}
   * @private
   */
  shouldShowCollections_(path) {
    return path === Paths.Collections;
  }

  /**
   * Navigate to the selected collection id. Assumes validation of the
   * collection id has already happened.
   * @param {!string} collectionId
   * @private
   */
  selectCollection_(collectionId) {
    this.setProperties(
        {path_: Paths.CollectionImages, queryParams_: {id: collectionId}});
  }

  /**
   * @param {string} path
   * @return {boolean}
   * @private
   */
  shouldShowCollectionImages_(path) {
    return path === Paths.CollectionImages;
  }
}

customElements.define(PersonalizationRouter.is, PersonalizationRouter);

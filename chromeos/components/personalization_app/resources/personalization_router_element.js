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

  /**
   * @param {string} path
   * @return {boolean}
   * @private
   */
  showCollections_(path) {
    return path === '/';
  }

  /**
   * @param {string} path
   * @return {boolean}
   * @private
   */
  showCollectionImages_(path) {
    return path === '/collection';
  }
}

customElements.define(PersonalizationRouter.is, PersonalizationRouter);

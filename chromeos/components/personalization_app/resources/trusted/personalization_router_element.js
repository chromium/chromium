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
export const Paths = {
  CollectionImages: '/collection',
  Collections: '/',
  LocalCollection: '/local',
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

  static instance() {
    return document.querySelector(PersonalizationRouter.is);
  }

  /**
   * Reload the application at the collections page.
   */
  static reloadAtRoot() {
    window.location.replace(Paths.Collections);
  }

  get collectionId() {
    if (this.path_ !== Paths.CollectionImages) {
      return null;
    }
    return this.queryParams_.id;
  }

  /**
   * Navigate to the selected collection id. Assumes validation of the
   * collection has already happened.
   * @param {!chromeos.personalizationApp.mojom.WallpaperCollection} collection
   */
  selectCollection(collection) {
    document.title = collection.name;
    this.setProperties(
        {path_: Paths.CollectionImages, queryParams_: {id: collection.id}});
  }

  /**
   * Navigate to the local collection page.
   */
  selectLocalCollection() {
    this.setProperties({path_: Paths.LocalCollection, query_: ''});
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
   * @param {string} path
   * @return {boolean}
   * @private
   */
  shouldShowCollectionImages_(path) {
    return path === Paths.CollectionImages;
  }

  /**
   * @param {string} path
   * @return  {boolean}
   * @private
   */
  shouldShowLocalCollection_(path) {
    return path === Paths.LocalCollection;
  }
}

customElements.define(PersonalizationRouter.is, PersonalizationRouter);

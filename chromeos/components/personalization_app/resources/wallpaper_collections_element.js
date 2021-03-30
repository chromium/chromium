// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Polymer element that fetches and displays a list of WallpaperCollection
 * objects. Currently a stub element to confirm polymer and mojom are working.
 * TODO(b/182012641) move full functionality here from
 * chrome://os-settings/wallpaper code.
 */

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getWallpaperProvider} from './mojo_interface_provider.js';

export default class WallpaperCollections extends PolymerElement {
  static get is() {
    return 'wallpaper-collections';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      initialized_: {
        type: Boolean,
        value: false,
      }
    };
  }

  constructor() {
    super();
    /** @private */
    this.wallpaperProvider_ = getWallpaperProvider();
  }

  ready() {
    super.ready();
    this.wallpaperProvider_.initialize().then(
        (response) => this.initialized_ = response.success,
        (error) => console.error('Error initializing', error),
    );
  }
}

customElements.define(WallpaperCollections.is, WallpaperCollections);

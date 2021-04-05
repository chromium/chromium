// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from '../../chai_assert.js';
import {TestBrowserProxy} from '../../test_browser_proxy.m.js';

/**
 * @implements {chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 * @extends {TestBrowserProxy}
 */
export class TestWallpaperProvider extends TestBrowserProxy {
  constructor() {
    super([
      'fetchCollections',
      'fetchImagesForCollection',
    ]);

    /**
     * @private
     * @type {?Array<!chromeos.personalizationApp.mojom.WallpaperCollection>}
     */
    this.collections_ = [{id: 'id_0', name: 'zero'}, {id: 'id_1', name: 'one'}];

    /**
     * @private
     * @type {?Array<!chromeos.personalizationApp.mojom.WallpaperImage>}
     */
    this.images_ = [
      {url: {url: 'https://url_0/'}},
      {url: {url: 'https://url_1/'}},
    ];
  }

  /** @override */
  fetchCollections() {
    this.methodCalled('fetchCollections');
    return Promise.resolve({collections: this.collections_});
  }

  /** @override */
  fetchImagesForCollection(collectionId) {
    this.methodCalled('fetchImagesForCollection', collectionId);
    assertTrue(
        !!this.collections_.find(({id}) => id === collectionId),
        'Must request images for existing wallpaper collection',
    );
    return Promise.resolve({images: this.images_});
  }

  /**
   * @param {!Array<!chromeos.personalizationApp.mojom.WallpaperCollection>}
   *     collections
   */
  setCollections(collections) {
    this.collections_ = collections;
  }

  setCollectionsToFail() {
    this.collections_ = null;
  }

  /**
   * @param {Array<!chromeos.personalizationApp.mojom.WallpaperImage>} images
   */
  setImages(images) {
    this.images_ = images;
  }

  setImagesToFail() {
    this.images_ = null;
  }
}

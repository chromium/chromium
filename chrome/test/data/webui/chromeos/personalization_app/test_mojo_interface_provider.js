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
      'selectWallpaper',
    ]);

    /**
     * URLs are not real but must have the correct origin to pass CSP checks.
     * @private
     * @type {?Array<!chromeos.personalizationApp.mojom.WallpaperCollection>}
     */
    this.collections_ = [
      {
        id: 'id_0',
        name: 'zero',
        preview: {url: 'https://collections.googleusercontent.com/0'}
      },
      {
        id: 'id_1',
        name: 'one',
        preview: {url: 'https://collections.googleusercontent.com/1'}
      },
    ];

    /**
     * URLs are not real but must have the correct origin to pass CSP checks.
     * @private
     * @type {?Array<!chromeos.personalizationApp.mojom.WallpaperImage>}
     */
    this.images_ = [
      {
        url: {url: 'https://images.googleusercontent.com/0'},
        assetId: BigInt(0),
      },
      {
        url: {url: 'https://images.googleusercontent.com/1'},
        assetId: BigInt(1),
      },
    ];

    /** @public */
    this.selectWallpaperResponse = true;
  }

  /**
   * @return {?Array<!chromeos.personalizationApp.mojom.WallpaperCollection>}
   */
  get collections() {
    return this.collections_;
  }

  /**
   * @return {?Array<!chromeos.personalizationApp.mojom.WallpaperImage>}
   */
  get images() {
    return this.images_;
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

  /** @override */
  selectWallpaper(assetId) {
    this.methodCalled('selectWallpaper', assetId);
    return Promise.resolve({success: this.selectWallpaperResponse});
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

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {unguessableTokenToString} from 'chrome://personalization/common/utils.js';
import {assertTrue} from '../../chai_assert.js';
import {TestBrowserProxy} from '../../test_browser_proxy.js';

/**
 * @implements {chromeos.personalizationApp.mojom.WallpaperProviderInterface}
 * @extends {TestBrowserProxy}
 */
export class TestWallpaperProvider extends TestBrowserProxy {
  constructor() {
    super([
      'fetchCollections',
      'fetchImagesForCollection',
      'getLocalImages',
      'getLocalImageThumbnail',
      'setWallpaperObserver',
      'selectWallpaper',
      'setDailyRefreshCollectionId',
      'getDailyRefreshCollectionId',
      'updateDailyRefreshWallpaper',
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
        assetId: BigInt(0),
        attribution: ['Image 0'],
        url: {url: 'https://images.googleusercontent.com/0'},
      },
      {
        assetId: BigInt(1),
        attribution: ['Image 1'],
        url: {url: 'https://images.googleusercontent.com/1'},
      },
    ];

    /** @type {?Array<!chromeos.personalizationApp.mojom.LocalImage>} */
    this.localImages = [
      {
        id: {high: BigInt(100), low: BigInt(10)},
        name: 'LocalImage0',
      },
      {
        id: {high: BigInt(200), low: BigInt(20)},
        name: 'LocalImage1',
      }
    ];

    /** @type {!Object<string, string>} */
    this.localImageData = {
      '100,10': 'data://localimage0data',
      '200,20': 'data://localimage1data',
    };

    /**
     * @public
     * @type {?chromeos.personalizationApp.mojom.CurrentWallpaper}
     */
    this.currentWallpaper = {
      attribution: ['Image 0'],
      layout: chromeos.personalizationApp.mojom.WallpaperLayout.kCenter,
      key: '1',
      type: chromeos.personalizationApp.mojom.WallpaperType.kOnline,
      url: {url: 'https://images.googleusercontent.com/0'},
    };

    /** @public */
    this.selectWallpaperResponse = true;

    /** @public */
    this.selectLocalImageResponse = true;

    /** @public */
    this.updateDailyRefreshWallpaperResponse = true;

    /**
     * @public
     * @type {?chromeos.personalizationApp.mojom.WallpaperObserverInterface}
     */
    this.wallpaperObserverRemote = null;
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
  getLocalImages() {
    this.methodCalled('getLocalImages');
    return Promise.resolve({images: this.localImages});
  }

  /** @override */
  getLocalImageThumbnail(id) {
    this.methodCalled('getLocalImageThumbnail', id);
    return Promise.resolve(
        {data: this.localImageData[unguessableTokenToString(id)]});
  }

  /** @override */
  setWallpaperObserver(remote) {
    this.methodCalled('setWallpaperObserver');
    this.wallpaperObserverRemote = remote;
    this.wallpaperObserverRemote.onWallpaperChanged(this.currentWallpaper);
  }

  /** @override */
  selectWallpaper(assetId) {
    this.methodCalled('selectWallpaper', assetId);
    return Promise.resolve({success: this.selectWallpaperResponse});
  }

  /** @override */
  selectLocalImage(id) {
    this.methodCalled('selectLocalImage', id);
    return Promise.resolve({success: this.selectLocalImageResponse});
  }

  /** @override */
  setCustomWallpaperLayout(layout) {
    this.methodCalled('selectCustomWallpaperLayout', layout);
  }

  /** @override */
  setDailyRefreshCollectionId(collectionId) {
    this.methodCalled('setDailyRefreshCollectionId', collectionId);
  }

  /** @override */
  getDailyRefreshCollectionId() {
    this.methodCalled('getDailyRefreshCollectionId');
    return Promise.resolve({collectionId: this.collections_[0].id});
  }

  /** @override */
  updateDailyRefreshWallpaper() {
    this.methodCalled('updateDailyRefreshWallpaper');
    return Promise.resolve({success: this.updateDailyRefreshWallpaperResponse});
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

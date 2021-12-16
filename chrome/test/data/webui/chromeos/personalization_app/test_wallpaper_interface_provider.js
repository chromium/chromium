// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OnlineImageType, WallpaperLayout, WallpaperType} from 'chrome://personalization/trusted/personalization_app.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {assertTrue} from '../../chai_assert.js';
import {TestBrowserProxy} from '../../test_browser_proxy.js';

/**
 * @implements {WallpaperProviderInterface}
 * @extends {TestBrowserProxy}
 */
export class TestWallpaperProvider extends TestBrowserProxy {
  constructor() {
    super([
      'makeTransparent',
      'fetchCollections',
      'fetchImagesForCollection',
      'fetchGooglePhotosCount',
      'getLocalImages',
      'getLocalImageThumbnail',
      'setWallpaperObserver',
      'selectWallpaper',
      'selectLocalImage',
      'setCustomWallpaperLayout',
      'setDailyRefreshCollectionId',
      'getDailyRefreshCollectionId',
      'updateDailyRefreshWallpaper',
      'isInTabletMode',
      'confirmPreviewWallpaper',
      'cancelPreviewWallpaper',
    ]);

    /**
     * URLs are not real but must have the correct origin to pass CSP checks.
     * @private
     * @type {?Array<!WallpaperCollection>}
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
     * @type {?Array<!WallpaperImage>}
     */
    this.images_ = [
      {
        assetId: BigInt(0),
        attribution: ['Image 0'],
        url: {url: 'https://images.googleusercontent.com/0'},
        unitId: BigInt(1),
        type: OnlineImageType.kDark,
      },
      {
        assetId: BigInt(2),
        attribution: ['Image 2'],
        url: {url: 'https://images.googleusercontent.com/2'},
        unitId: BigInt(2),
        type: OnlineImageType.kDark,
      },
      {
        assetId: BigInt(1),
        attribution: ['Image 1'],
        url: {url: 'https://images.googleusercontent.com/1'},
        unitId: BigInt(1),
        type: OnlineImageType.kLight,
      },
    ];

    /** @type {?Array<!mojoBase.mojom.FilePath>} */
    this.localImages = [{path: 'LocalImage0.png'}, {path: 'LocalImage1.png'}];

    /** @type {!Object<string, string>} */
    this.localImageData = {
      'LocalImage0.png': 'data://localimage0data',
      'LocalImage1.png': 'data://localimage1data',
    };

    /**
     * @public
     * @type {?CurrentWallpaper}
     */
    this.currentWallpaper = {
      attribution: ['Image 0'],
      layout: WallpaperLayout.kCenter,
      key: '1',
      type: WallpaperType.kOnline,
      url: {url: 'https://images.googleusercontent.com/0'},
    };

    /** @public */
    this.selectWallpaperResponse = true;

    /** @public */
    this.selectLocalImageResponse = true;

    /** @public */
    this.updateDailyRefreshWallpaperResponse = true;

    /** @public */
    this.isInTabletModeResponse = true;

    /** @public */
    this.wallpaperObserverUpdateTimeout = 0;

    /**
     * @public
     * @type {?WallpaperObserverInterface}
     */
    this.wallpaperObserverRemote = null;
  }

  /**
   * @return {?Array<!WallpaperCollection>}
   */
  get collections() {
    return this.collections_;
  }

  /**
   * @return {?Array<!WallpaperImage>}
   */
  get images() {
    return this.images_;
  }

  /** @override */
  makeTransparent() {
    this.methodCalled('makeTransparent');
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
  fetchGooglePhotosCount() {
    this.methodCalled('fetchGooglePhotosCount');
    const count =
        loadTimeData.getBoolean('isGooglePhotosIntegrationEnabled') ? 0 : -1;
    return Promise.resolve({count: count});
  }

  /** @override */
  getLocalImages() {
    this.methodCalled('getLocalImages');
    return Promise.resolve({images: this.localImages});
  }

  /** @override */
  getLocalImageThumbnail(filePath) {
    this.methodCalled('getLocalImageThumbnail', filePath);
    return Promise.resolve({data: this.localImageData[filePath.path]});
  }

  /** @override */
  setWallpaperObserver(remote) {
    this.methodCalled('setWallpaperObserver');
    this.wallpaperObserverRemote = remote;
    window.setTimeout(() => {
      this.wallpaperObserverRemote.onWallpaperChanged(this.currentWallpaper);
    }, this.wallpaperObserverUpdateTimeout);
  }

  /** @override */
  selectWallpaper(assetId, previewMode) {
    this.methodCalled('selectWallpaper', assetId, previewMode);
    return Promise.resolve({success: this.selectWallpaperResponse});
  }

  /** @override */
  selectLocalImage(id, layout, previewMode) {
    this.methodCalled('selectLocalImage', id, layout, previewMode);
    return Promise.resolve({success: this.selectLocalImageResponse});
  }

  /** @override */
  setCustomWallpaperLayout(layout) {
    this.methodCalled('setCustomWallpaperLayout', layout);
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

  isInTabletMode() {
    this.methodCalled('isInTabletMode');
    return Promise.resolve({tabletMode: this.isInTabletModeResponse});
  }

  /** @override */
  confirmPreviewWallpaper() {
    this.methodCalled('confirmPreviewWallpaper');
  }

  /** @override */
  cancelPreviewWallpaper() {
    this.methodCalled('cancelPreviewWallpaper');
  }

  /**
   * @param {!Array<!WallpaperCollection>}
   *     collections
   */
  setCollections(collections) {
    this.collections_ = collections;
  }

  setCollectionsToFail() {
    this.collections_ = null;
  }

  /**
   * @param {Array<!WallpaperImage>} images
   */
  setImages(images) {
    this.images_ = images;
  }

  setImagesToFail() {
    this.images_ = null;
  }
}

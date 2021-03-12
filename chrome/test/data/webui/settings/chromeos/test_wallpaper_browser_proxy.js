// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {TestBrowserProxy} from '../../test_browser_proxy.m.js';
// #import {assertTrue} from '../../chai_assert.js';

cr.define('settings', function() {
  /** @implements {settings.WallpaperBrowserProxy} */
  /* #export */ class TestWallpaperBrowserProxy extends TestBrowserProxy {
    constructor() {
      super([
        'isWallpaperSettingVisible',
        'isWallpaperPolicyControlled',
        'openWallpaperManager',
        'fetchWallpaperCollections',
        'fetchImagesForCollection',
      ]);

      /** @private */
      this.isWallpaperSettingVisible_ = true;

      /** @private */
      this.isWallpaperPolicyControlled_ = false;

      /**
       * @private
       * @type {Array<!WallpaperCollection>}
       */
      this.wallpaperCollections_ =
          [{id: 'id_0', name: 'zero'}, {id: 'id_1', name: 'one'}];

      /**
       * @private
       * @type {Array<!WallpaperImage>}
       */
      this.wallpaperImages_ = [
        {url: 'https://url_0/'},
        {url: 'https://url_1/'},
      ];
    }

    /** @override */
    isWallpaperSettingVisible() {
      this.methodCalled('isWallpaperSettingVisible');
      return Promise.resolve(true);
    }

    /** @override */
    isWallpaperPolicyControlled() {
      this.methodCalled('isWallpaperPolicyControlled');
      return Promise.resolve(this.isWallpaperPolicyControlled_);
    }

    /** @override */
    openWallpaperManager() {
      this.methodCalled('openWallpaperManager');
    }

    /** @override */
    fetchWallpaperCollections() {
      this.methodCalled('fetchWallpaperCollections');
      return this.wallpaperCollections_.length ?
          Promise.resolve(this.wallpaperCollections_) :
          Promise.reject(null);
    }

    /** @override */
    fetchImagesForCollection(collectionId) {
      this.methodCalled('fetchImagesForCollection', collectionId);
      assertTrue(
          !!this.wallpaperCollections_.find(({id}) => id === collectionId),
          'Must request images for existing wallpaper collection',
      );

      return this.wallpaperImages_.length ?
          Promise.resolve(this.wallpaperImages_) :
          Promise.reject(null);
    }

    /** @param {boolean} Whether the wallpaper is policy controlled. */
    setIsWallpaperPolicyControlled(isPolicyControlled) {
      this.isWallpaperPolicyControlled_ = isPolicyControlled;
    }

    /** @param {Array<!WallpaperCollection>} */
    setWallpaperCollections(wallpaperCollections) {
      this.wallpaperCollections_ = wallpaperCollections;
    }

    /** @param {Array<!WallpaperImage>} */
    setWallpaperImages(images) {
      this.wallpaperImages_ = images;
    }
  }

  // #cr_define_end
  return {
    TestWallpaperBrowserProxy: TestWallpaperBrowserProxy,
  };
});
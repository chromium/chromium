// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CurrentWallpaper, FetchGooglePhotosAlbumsResponse, FetchGooglePhotosPhotosResponse, GooglePhotosAlbum, GooglePhotosPhoto, OnlineImageType, WallpaperCollection, WallpaperImage, WallpaperLayout, WallpaperObserverInterface, WallpaperObserverRemote, WallpaperProviderInterface, WallpaperType} from 'chrome://personalization/trusted/personalization_app.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestWallpaperProvider extends
    TestBrowserProxy<WallpaperProviderInterface> implements
        WallpaperProviderInterface {
  constructor() {
    super([
      'makeTransparent',
      'fetchCollections',
      'fetchImagesForCollection',
      'fetchGooglePhotosAlbums',
      'fetchGooglePhotosCount',
      'fetchGooglePhotosPhotos',
      'getLocalImages',
      'getLocalImageThumbnail',
      'setWallpaperObserver',
      'selectGooglePhotosPhoto',
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

    this.localImages = [{path: 'LocalImage0.png'}, {path: 'LocalImage1.png'}];

    this.localImageData = {
      'LocalImage0.png': 'data://localimage0data',
      'LocalImage1.png': 'data://localimage1data',
    };

    this.currentWallpaper = {
      attribution: ['Image 0'],
      layout: WallpaperLayout.kCenter,
      key: '1',
      type: WallpaperType.kOnline,
      url: {url: 'https://images.googleusercontent.com/0'},
    };
  }

  private collections_: WallpaperCollection[]|null;
  private images_: WallpaperImage[]|null;
  private googlePhotosAlbums_: GooglePhotosAlbum[]|undefined = [];
  private googlePhotosCount_: number = 0;
  private googlePhotosPhotos_: GooglePhotosPhoto[]|undefined = [];
  private googlePhotosPhotosByAlbumId_:
      Record<string, GooglePhotosPhoto[]|undefined> = {};
  localImages: FilePath[]|null;
  localImageData: Record<string, string>;
  currentWallpaper: CurrentWallpaper;
  selectWallpaperResponse = true;
  selectLocalImageResponse = true;
  updateDailyRefreshWallpaperResponse = true;
  isInTabletModeResponse = true;
  wallpaperObserverUpdateTimeout = 0;
  wallpaperObserverRemote: WallpaperObserverInterface|null = null;

  get collections(): WallpaperCollection[]|null {
    return this.collections_;
  }

  get images(): WallpaperImage[]|null {
    return this.images_;
  }

  makeTransparent() {
    this.methodCalled('makeTransparent');
  }

  fetchCollections() {
    this.methodCalled('fetchCollections');
    return Promise.resolve({collections: this.collections_});
  }

  fetchImagesForCollection(collectionId: string) {
    this.methodCalled('fetchImagesForCollection', collectionId);
    assertTrue(
        !!this.collections_ &&
            !!this.collections_.find(({id}) => id === collectionId),
        'Must request images for existing wallpaper collection',
    );
    return Promise.resolve({images: this.images_});
  }

  fetchGooglePhotosAlbums() {
    this.methodCalled('fetchGooglePhotosAlbums');
    const response = new FetchGooglePhotosAlbumsResponse();
    response.albums =
        loadTimeData.getBoolean('isGooglePhotosIntegrationEnabled') ?
        this.googlePhotosAlbums_ :
        undefined;
    response.resumeToken = undefined;
    return Promise.resolve({response});
  }

  fetchGooglePhotosCount() {
    this.methodCalled('fetchGooglePhotosCount');
    const count = loadTimeData.getBoolean('isGooglePhotosIntegrationEnabled') ?
        this.googlePhotosCount_ :
        -1;
    return Promise.resolve({count});
  }

  fetchGooglePhotosPhotos(itemId: string, albumId: string) {
    this.methodCalled('fetchGooglePhotosPhotos', itemId, albumId);
    const response = new FetchGooglePhotosPhotosResponse();
    response.photos =
        loadTimeData.getBoolean('isGooglePhotosIntegrationEnabled') ?
        albumId ? this.googlePhotosPhotosByAlbumId_[albumId] :
                  this.googlePhotosPhotos_ :
        undefined;
    response.resumeToken = undefined;
    return Promise.resolve({response});
  }

  getLocalImages() {
    this.methodCalled('getLocalImages');
    return Promise.resolve({images: this.localImages});
  }

  getLocalImageThumbnail(filePath: FilePath) {
    this.methodCalled('getLocalImageThumbnail', filePath);
    return Promise.resolve({data: this.localImageData[filePath.path]!});
  }

  setWallpaperObserver(remote: WallpaperObserverRemote) {
    this.methodCalled('setWallpaperObserver');
    this.wallpaperObserverRemote = remote;
    window.setTimeout(() => {
      this.wallpaperObserverRemote!.onWallpaperChanged(this.currentWallpaper);
    }, this.wallpaperObserverUpdateTimeout);
  }

  selectWallpaper(assetId: bigint, previewMode: boolean) {
    this.methodCalled('selectWallpaper', assetId, previewMode);
    return Promise.resolve({success: this.selectWallpaperResponse});
  }

  selectGooglePhotosPhoto(id: string) {
    this.methodCalled('selectGooglePhotosPhoto', id);
    return Promise.resolve({success: false});
  }

  selectLocalImage(
      path: FilePath, layout: WallpaperLayout, previewMode: boolean) {
    this.methodCalled('selectLocalImage', path, layout, previewMode);
    return Promise.resolve({success: this.selectLocalImageResponse});
  }

  setCustomWallpaperLayout(layout: WallpaperLayout) {
    this.methodCalled('setCustomWallpaperLayout', layout);
  }

  setDailyRefreshCollectionId(collectionId: string) {
    this.methodCalled('setDailyRefreshCollectionId', collectionId);
  }

  getDailyRefreshCollectionId() {
    this.methodCalled('getDailyRefreshCollectionId');
    return Promise.resolve({collectionId: this.collections_![0]!.id});
  }

  updateDailyRefreshWallpaper() {
    this.methodCalled('updateDailyRefreshWallpaper');
    return Promise.resolve({success: this.updateDailyRefreshWallpaperResponse});
  }

  isInTabletMode() {
    this.methodCalled('isInTabletMode');
    return Promise.resolve({tabletMode: this.isInTabletModeResponse});
  }

  confirmPreviewWallpaper() {
    this.methodCalled('confirmPreviewWallpaper');
  }

  cancelPreviewWallpaper() {
    this.methodCalled('cancelPreviewWallpaper');
  }

  setCollections(collections: WallpaperCollection[]) {
    this.collections_ = collections;
  }

  setCollectionsToFail() {
    this.collections_ = null;
  }

  setGooglePhotosAlbums(googlePhotosAlbums: GooglePhotosAlbum[]|undefined) {
    this.googlePhotosAlbums_ = googlePhotosAlbums;
  }

  setGooglePhotosCount(googlePhotosCount: number) {
    this.googlePhotosCount_ = googlePhotosCount;
  }

  setGooglePhotosPhotos(googlePhotosPhotos: GooglePhotosPhoto[]|undefined) {
    this.googlePhotosPhotos_ = googlePhotosPhotos;
  }

  setGooglePhotosPhotosByAlbumId(
      albumId: string, googlePhotosPhotos: GooglePhotosPhoto[]|undefined) {
    this.googlePhotosPhotosByAlbumId_[albumId] = googlePhotosPhotos;
  }

  setImages(images: WallpaperImage[]) {
    this.images_ = images;
  }

  setImagesToFail() {
    this.images_ = null;
  }
}

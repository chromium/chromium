// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CurrentAttribution, CurrentWallpaper, DefaultImageSymbol, GooglePhotosAlbum, GooglePhotosEnablementState, GooglePhotosPhoto, kDefaultImageSymbol, OnlineImageType, WallpaperCollection, WallpaperImage, WallpaperLayout, WallpaperObserverInterface, WallpaperObserverRemote, WallpaperProviderInterface, WallpaperType} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestWallpaperProvider extends TestBrowserProxy implements
    WallpaperProviderInterface {
  constructor() {
    super([
      'makeTransparent',
      'makeOpaque',
      'fetchCollections',
      'fetchImagesForCollection',
      'fetchGooglePhotosAlbums',
      'fetchGooglePhotosEnabled',
      'fetchGooglePhotosPhotos',
      'fetchGooglePhotosSharedAlbums',
      'getDefaultImageThumbnail',
      'getLocalImages',
      'getLocalImageThumbnail',
      'setWallpaperObserver',
      'selectGooglePhotosPhoto',
      'selectGooglePhotosAlbum',
      'getGooglePhotosDailyRefreshAlbumId',
      'selectWallpaper',
      'selectDefaultImage',
      'selectLocalImage',
      'setCurrentWallpaperLayout',
      'setDailyRefreshCollectionId',
      'getDailyRefreshCollectionId',
      'updateDailyRefreshWallpaper',
      'isInTabletMode',
      'confirmPreviewWallpaper',
      'cancelPreviewWallpaper',
      'shouldShowTimeOfDayWallpaperDialog',
    ]);

    /**
     * URLs are not real but must have the correct origin to pass CSP checks.
     */
    this.collections_ = [
      {
        descriptionContent: 'description for collection zero',
        id: 'id_0',
        name: 'zero',
        previews: [{url: 'https://collections.googleusercontent.com/0'}],
      },
      {
        descriptionContent: '',
        id: 'id_1',
        name: 'one',
        previews: [{url: 'https://collections.googleusercontent.com/1'}],
      },
      {
        descriptionContent: '',
        id: 'id_2',
        name: 'dark-light',
        previews: [
          {url: 'https://collections.googleusercontent.com/2'},
          {url: 'https://collections.googleusercontent.com/3'},
        ],
      },
      {
        descriptionContent: '',
        id: loadTimeData.getString('timeOfDayWallpaperCollectionId'),
        name: 'time-of-day',
        previews: [
          {url: 'https://collections.googleusercontent.com/tod'},
        ],
      },
    ];

    /**
     * URLs are not real but must have the correct origin to pass CSP checks.
     */
    this.images_ = [
      {
        assetId: BigInt(1),
        attribution: ['Image 0 light'],
        url: {url: 'https://images.googleusercontent.com/1'},
        unitId: BigInt(1),
        type: OnlineImageType.kLight,
      },
      {
        assetId: BigInt(2),
        attribution: ['Image 2'],
        url: {url: 'https://images.googleusercontent.com/2'},
        unitId: BigInt(2),
        type: OnlineImageType.kUnknown,
      },
      {
        assetId: BigInt(0),
        attribution: ['Image 0 dark'],
        url: {url: 'https://images.googleusercontent.com/0'},
        unitId: BigInt(1),
        type: OnlineImageType.kDark,
      },
      {
        assetId: BigInt(3),
        attribution: ['Image 3'],
        url: {url: 'https://images.googleusercontent.com/light-1'},
        unitId: BigInt(3),
        type: OnlineImageType.kLight,
      },
      {
        assetId: BigInt(4),
        attribution: ['Image 3'],
        url: {url: 'https://images.googleusercontent.com/morning-1'},
        unitId: BigInt(3),
        type: OnlineImageType.kMorning,
      },
      {
        assetId: BigInt(5),
        attribution: ['Image 3'],
        url: {url: 'https://images.googleusercontent.com/afternoon-1'},
        unitId: BigInt(3),
        type: OnlineImageType.kLateAfternoon,
      },
      {
        assetId: BigInt(6),
        attribution: ['Image 3'],
        url: {url: 'https://images.googleusercontent.com/dark-1'},
        unitId: BigInt(3),
        type: OnlineImageType.kDark,
      },
    ];

    this.localImages = [{path: 'LocalImage0.png'}, {path: 'LocalImage1.png'}];

    this.localImageData = {
      [kDefaultImageSymbol]: {url: ''},
      'LocalImage0.png': {url: 'data:image/png;base64,localimage0data'},
      'LocalImage1.png': {url: 'data:image/png;base64,localimage1data'},
    };

    this.attribution = {
      attribution: ['Image 0 light'],
      key: '1',
    };

    this.currentWallpaper = {
      descriptionContent: 'test content',
      descriptionTitle: 'test title',
      key: '1',
      layout: WallpaperLayout.kCenter,
      type: WallpaperType.kOnline,
    };

    this.albumId = '';

    this.collectionId = this.collections_![0]!.id;
    this.timeOfDayCollectionId = this.collections_![3]!.id;
  }

  private collections_: WallpaperCollection[]|null;
  private images_: WallpaperImage[]|null;
  private googlePhotosAlbums_: GooglePhotosAlbum[]|null = [];
  private googlePhotosAlbumsResumeToken_: string|null = null;
  private googlePhotosSharedAlbums_: GooglePhotosAlbum[]|null = [];
  private googlePhotosSharedAlbumsResumeToken_: string|null = null;
  private googlePhotosEnabled_: GooglePhotosEnablementState =
      GooglePhotosEnablementState.kEnabled;
  private googlePhotosPhotos_: GooglePhotosPhoto[]|null = [];
  private googlePhotosPhotosResumeToken_: string|null = null;
  private googlePhotosPhotosByAlbumId_:
      Record<string, GooglePhotosPhoto[]|null> = {};
  private googlePhotosPhotosByAlbumIdResumeTokens_:
      Record<string, string|null> = {};
  localImages: FilePath[]|null;
  localImageData: Record<string|DefaultImageSymbol, Url>;
  defaultImageThumbnail:
      Url = {url: 'data:image/png;base64,default_image_thumbnail'};
  attribution: CurrentAttribution;
  currentWallpaper: CurrentWallpaper;
  albumId: string;
  collectionId: string;
  setDailyRefreshCollectionIdResponse = {success: false};
  timeOfDayCollectionId: string;
  selectWallpaperResponse = true;
  selectGooglePhotosPhotoResponse = true;
  selectGooglePhotosAlbumResponse = true;
  selectDefaultImageResponse = true;
  selectLocalImageResponse = true;
  updateDailyRefreshWallpaperResponse = true;
  isInTabletModeResponse = true;
  shouldShowTimeOfDayWallpaperDialogResponse = true;
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

  makeOpaque() {
    this.methodCalled('makeOpaque');
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

  fetchGooglePhotosAlbums(resumeToken: string|null) {
    this.methodCalled('fetchGooglePhotosAlbums', resumeToken);
    const albums = loadTimeData.getBoolean('isGooglePhotosIntegrationEnabled') ?
        this.googlePhotosAlbums_ :
        null;
    const token = this.googlePhotosAlbumsResumeToken_;
    return Promise.resolve({response: {albums, resumeToken: token}});
  }

  fetchGooglePhotosSharedAlbums(resumeToken: string|null) {
    this.methodCalled('fetchGooglePhotosSharedAlbums', resumeToken);
    const albums = this.googlePhotosSharedAlbums_;
    const token = this.googlePhotosSharedAlbumsResumeToken_;
    return Promise.resolve({response: {albums, resumeToken: token}});
  }

  fetchGooglePhotosEnabled() {
    this.methodCalled('fetchGooglePhotosEnabled');
    const state = loadTimeData.getBoolean('isGooglePhotosIntegrationEnabled') ?
        this.googlePhotosEnabled_ :
        GooglePhotosEnablementState.kError;
    return Promise.resolve({state});
  }

  fetchGooglePhotosPhotos(
      itemId: string, albumId: string, resumeToken: string) {
    this.methodCalled('fetchGooglePhotosPhotos', itemId, albumId, resumeToken);
    const photos = loadTimeData.getBoolean('isGooglePhotosIntegrationEnabled') ?
        albumId ? this.googlePhotosPhotosByAlbumId_[albumId]! :
                  this.googlePhotosPhotos_ :
        null;
    const token = albumId ?
        this.googlePhotosPhotosByAlbumIdResumeTokens_[albumId]! :
        this.googlePhotosPhotosResumeToken_;
    return Promise.resolve({response: {photos, resumeToken: token}});
  }

  getDefaultImageThumbnail(): Promise<{data: Url}> {
    this.methodCalled('getDefaultImageThumbnail');
    return Promise.resolve({data: this.defaultImageThumbnail});
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
      this.wallpaperObserverRemote!.onAttributionChanged(this.attribution);
    }, this.wallpaperObserverUpdateTimeout);
  }

  selectWallpaper(assetId: bigint, previewMode: boolean) {
    this.methodCalled('selectWallpaper', assetId, previewMode);
    return Promise.resolve({success: this.selectWallpaperResponse});
  }

  selectDefaultImage() {
    this.methodCalled('selectDefaultImage');
    return Promise.resolve({success: this.selectDefaultImageResponse});
  }

  selectGooglePhotosPhoto(id: string) {
    this.methodCalled('selectGooglePhotosPhoto', id);
    return Promise.resolve({success: this.selectGooglePhotosPhotoResponse});
  }

  selectGooglePhotosAlbum(id: string) {
    this.methodCalled('selectGooglePhotosAlbum', id);
    return Promise.resolve({success: this.selectGooglePhotosAlbumResponse});
  }

  getGooglePhotosDailyRefreshAlbumId() {
    this.methodCalled('getGooglePhotosDailyRefreshAlbumId');
    return Promise.resolve({albumId: this.albumId});
  }

  selectLocalImage(
      path: FilePath, layout: WallpaperLayout, previewMode: boolean) {
    this.methodCalled('selectLocalImage', path, layout, previewMode);
    return Promise.resolve({success: this.selectLocalImageResponse});
  }

  setCurrentWallpaperLayout(layout: WallpaperLayout) {
    this.methodCalled('setCurrentWallpaperLayout', layout);
  }

  setDailyRefreshCollectionId(collectionId: string) {
    this.methodCalled('setDailyRefreshCollectionId', collectionId);
    return Promise.resolve(this.setDailyRefreshCollectionIdResponse);
  }

  getDailyRefreshCollectionId() {
    this.methodCalled('getDailyRefreshCollectionId');
    return Promise.resolve({collectionId: this.collectionId});
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

  shouldShowTimeOfDayWallpaperDialog() {
    this.methodCalled('shouldShowTimeOfDayWallpaperDialog');
    return Promise.resolve(
        {shouldShowDialog: this.shouldShowTimeOfDayWallpaperDialogResponse});
  }

  setCollections(collections: WallpaperCollection[]) {
    this.collections_ = collections;
  }

  setCollectionsToFail() {
    this.collections_ = null;
  }

  setGooglePhotosAlbums(googlePhotosAlbums: GooglePhotosAlbum[]|null) {
    this.googlePhotosAlbums_ = googlePhotosAlbums;
  }

  setGooglePhotosAlbumsResumeToken(googlePhotosAlbumsResumeToken: string|null) {
    this.googlePhotosAlbumsResumeToken_ = googlePhotosAlbumsResumeToken;
  }

  setGooglePhotosSharedAlbums(googlePhotosSharedAlbums: GooglePhotosAlbum[]|
                              null) {
    this.googlePhotosSharedAlbums_ = googlePhotosSharedAlbums;
  }

  setGooglePhotosSharedAlbumsResumeToken(googlePhotosSharedAlbumsResumeToken:
                                             string|null) {
    this.googlePhotosSharedAlbumsResumeToken_ =
        googlePhotosSharedAlbumsResumeToken;
  }

  setGooglePhotosEnabled(googlePhotosEnabled: GooglePhotosEnablementState) {
    this.googlePhotosEnabled_ = googlePhotosEnabled;
  }

  setGooglePhotosPhotos(googlePhotosPhotos: GooglePhotosPhoto[]|null) {
    this.googlePhotosPhotos_ = googlePhotosPhotos;
  }

  setGooglePhotosPhotosResumeToken(googlePhotosPhotosResumeToken: string|null) {
    this.googlePhotosPhotosResumeToken_ = googlePhotosPhotosResumeToken;
  }

  setGooglePhotosPhotosByAlbumId(
      albumId: string, googlePhotosPhotos: GooglePhotosPhoto[]|null) {
    this.googlePhotosPhotosByAlbumId_[albumId] = googlePhotosPhotos;
  }

  setGooglePhotosPhotosByAlbumIdResumeToken(
      albumId: string, googlePhotosPhotosResumeToken: string|null) {
    this.googlePhotosPhotosByAlbumIdResumeTokens_[albumId] =
        googlePhotosPhotosResumeToken;
  }

  setImages(images: WallpaperImage[]) {
    this.images_ = images;
  }

  setImagesToFail() {
    this.images_ = null;
  }
}

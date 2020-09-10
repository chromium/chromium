// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {AmbientModeBrowserProxyImpl} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {TestBrowserProxy} from '../../test_browser_proxy.m.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

/**
 * @implements {settings.AmbientModeBrowserProxy}
 */
class TestAmbientModeBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'requestTopicSource',
      'requestAlbums',
      'setSelectedAlbums',
    ]);
  }

  /** @override */
  requestTopicSource() {
    this.methodCalled('requestTopicSource');
  }

  /** @override */
  requestAlbums(topicSource) {
    this.methodCalled('requestAlbums', [topicSource]);
  }

  setSelectedAlbums(settings) {
    this.methodCalled('setSelectedAlbums', [settings]);
  }
}

suite('AmbientModeHandler', function() {
  /** @type {SettingsAmbientModePhotosPageElement} */
  let ambientModePhotosPage = null;

  /** @type {?TestAmbientModeBrowserProxy} */
  let browserProxy = null;

  suiteSetup(function() {});

  setup(function() {
    browserProxy = new TestAmbientModeBrowserProxy();
    settings.AmbientModeBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();

    ambientModePhotosPage =
        document.createElement('settings-ambient-mode-photos-page');
    document.body.appendChild(ambientModePhotosPage);
    Polymer.dom.flush();
  });

  teardown(function() {
    ambientModePhotosPage.remove();
  });

  /**
   * @param {!AmbientModeTopicSource} topicSource
   * @private
   */
  function assertCheckPosition(topicSource) {
    ambientModePhotosPage.albums = [
      {albumId: 'id0', checked: true, title: 'album0', url: 'url'},
      {albumId: 'id1', checked: true, title: 'album1', url: 'url'}
    ];
    ambientModePhotosPage.topicSource = topicSource;
    Polymer.dom.flush();

    const albumList = ambientModePhotosPage.$$('album-list');
    const ironList = albumList.$$('iron-list');
    const albumItems = ironList.querySelectorAll('album-item:not([hidden])');

    albumItems.forEach((album) => {
      const check = album.$$('.check');
      const image = album.$$('#image');
      const boundingWidth = image.getBoundingClientRect().width;
      const scale = boundingWidth / image.offsetWidth;

      const checkTop = Math.round(
          (image.offsetHeight * (1.0 - scale) - check.offsetHeight) / 2.0);
      assertEquals(checkTop, check.offsetTop);

      const checkLeft = Math.round(
          (image.offsetWidth * (1.0 - scale) - check.offsetWidth) / 2.0 +
          boundingWidth);
      assertEquals(checkLeft, check.offsetLeft);
    });
  }

  test('hasAlbumsWithoutPhotoPreview', function() {
    // Disable photo preview feature and reload the |ambientModePhotosPage|.
    loadTimeData.overrideValues({isAmbientModePhotoPreviewEnabled: false});
    assertFalse(loadTimeData.getBoolean('isAmbientModePhotoPreviewEnabled'));

    ambientModePhotosPage.remove();
    ambientModePhotosPage =
        document.createElement('settings-ambient-mode-photos-page');
    document.body.appendChild(ambientModePhotosPage);

    ambientModePhotosPage.albums = [
      {albumId: 'id0', checked: true, title: 'album0'},
      {albumId: 'id1', checked: false, title: 'album1'}
    ];
    Polymer.dom.flush();

    const ironList = ambientModePhotosPage.$$('iron-list');
    const checkboxes = ironList.querySelectorAll('cr-checkbox');
    assertEquals(2, checkboxes.length);

    const checkbox0 = checkboxes[0];
    const checkbox1 = checkboxes[1];
    assertEquals('id0', checkbox0.dataset.id);
    assertTrue(checkbox0.checked);
    assertEquals('album0', checkbox0.label);
    assertEquals('id1', checkbox1.dataset.id);
    assertFalse(checkbox1.checked);
    assertEquals('album1', checkbox1.label);

    // Reset/enable photo preview feature.
    loadTimeData.overrideValues({isAmbientModePhotoPreviewEnabled: true});
    assertTrue(loadTimeData.getBoolean('isAmbientModePhotoPreviewEnabled'));
  });

  test('hasAlbumsWithPhotoPreview', function() {
    ambientModePhotosPage.albums = [
      {albumId: 'id0', checked: true, title: 'album0'},
      {albumId: 'id1', checked: false, title: 'album1'}
    ];
    Polymer.dom.flush();

    const albumList = ambientModePhotosPage.$$('album-list');
    const ironList = albumList.$$('iron-list');
    const albumItems = ironList.querySelectorAll('album-item:not([hidden])');
    assertEquals(2, albumItems.length);

    const album0 = albumItems[0];
    const album1 = albumItems[1];
    assertEquals('id0', album0.album.albumId);
    assertTrue(album0.album.checked);
    assertEquals('album0', album0.album.title);
    assertEquals('id1', album1.album.albumId);
    assertFalse(album1.album.checked);
    assertEquals('album1', album1.album.title);
  });

  test('personalPhotosImageContainerHasCorrectSize', function() {
    ambientModePhotosPage.albums = [
      {albumId: 'id0', checked: true, title: 'album0'},
      {albumId: 'id1', checked: false, title: 'album1'},
      {albumId: 'id2', checked: false, title: 'album2'}
    ];
    ambientModePhotosPage.topicSource = AmbientModeTopicSource.GOOGLE_PHOTOS;
    Polymer.dom.flush();

    const albumList = ambientModePhotosPage.$$('album-list');
    const ironList = albumList.$$('iron-list');
    assertTrue(ironList.grid);

    const albumItems = ironList.querySelectorAll('album-item:not([hidden])');
    assertEquals(3, albumItems.length);
    albumItems.forEach((album) => {
      const imageContainer = album.$$('#imageContainer');
      assertEquals(160, imageContainer.clientHeight);
      assertEquals(160, imageContainer.clientWidth);
    });
  });

  test('artImageContainerHasCorrectSize', function() {
    ambientModePhotosPage.albums = [
      {albumId: 'id0', checked: true, title: 'album0'},
      {albumId: 'id1', checked: false, title: 'album1'},
      {albumId: 'id2', checked: false, title: 'album2'}
    ];
    ambientModePhotosPage.topicSource = AmbientModeTopicSource.ART_GALLERY;
    Polymer.dom.flush();

    const albumList = ambientModePhotosPage.$$('album-list');
    const ironList = albumList.$$('iron-list');
    assertTrue(ironList.grid);

    const albumItems = ironList.querySelectorAll('album-item:not([hidden])');
    assertEquals(3, albumItems.length);
    albumItems.forEach((album) => {
      const imageContainer = album.$$('#imageContainer');
      assertEquals(160, imageContainer.clientHeight);
      assertEquals(256, imageContainer.clientWidth);
    });
  });

  test('toggleAlbumSelectionByClick', function() {
    ambientModePhotosPage.albums = [
      {albumId: 'id0', checked: true, title: 'album0', url: 'url'},
      {albumId: 'id1', checked: false, title: 'album1', url: 'url'}
    ];
    Polymer.dom.flush();

    const albumList = ambientModePhotosPage.$$('album-list');
    const ironList = albumList.$$('iron-list');
    const albumItems = ironList.querySelectorAll('album-item:not([hidden])');
    assertEquals(2, albumItems.length);

    const album0 = albumItems[0];
    const album1 = albumItems[1];
    assertTrue(album0.checked);
    assertFalse(album1.checked);

    // Verify that the selected-albums-changed event is sent when the album
    // image is clicked.
    let selectedAlbumsChangedEventCalls = 0;
    albumList.addEventListener('selected-albums-changed', (event) => {
      selectedAlbumsChangedEventCalls++;
    });

    // Click album item image will toggle the check.
    const image0 = album0.$$('#image');
    image0.click();
    assertFalse(album0.checked);
    assertEquals(1, selectedAlbumsChangedEventCalls);

    // Click album item image will toggle the check.
    image0.click();
    assertTrue(album0.checked);
    assertEquals(2, selectedAlbumsChangedEventCalls);

    // Click album item image will toggle the check.
    const image1 = album1.$$('#image');
    image1.click();
    assertTrue(album1.checked);
    assertEquals(3, selectedAlbumsChangedEventCalls);

    // Click album item image will toggle the check.
    image1.click();
    assertFalse(album1.checked);
    assertEquals(4, selectedAlbumsChangedEventCalls);
  });

  test('showCheckIconOnSelectedAlbum', function() {
    ambientModePhotosPage.albums = [
      {albumId: 'id0', checked: true, title: 'album0', url: 'url'},
      {albumId: 'id1', checked: false, title: 'album1', url: 'url'}
    ];
    Polymer.dom.flush();

    const albumList = ambientModePhotosPage.$$('album-list');
    const ironList = albumList.$$('iron-list');
    const albumItems = ironList.querySelectorAll('album-item:not([hidden])');

    const album0 = albumItems[0];
    const check0 = album0.$$('.check');
    assertTrue(album0.checked);
    assertFalse(check0.hidden);

    // Click album item image will toggle the check.
    album0.$$('#image').click();
    assertFalse(album0.checked);
    assertTrue(check0.hidden);

    const album1 = albumItems[1];
    const check1 = album1.$$('.check');
    assertFalse(album1.checked);
    assertTrue(check1.hidden);

    // Click album item image will toggle the check.
    album1.$$('#image').click();
    assertTrue(album1.checked);
    assertFalse(check1.hidden);
    // Click album1 will not affect album0.
    assertFalse(album0.checked);
    assertTrue(check0.hidden);
  });

  test('personalPhotosCheckIconHasCorrectPosition', function() {
    assertCheckPosition(AmbientModeTopicSource.GOOGLE_PHOTOS);
  });

  test('artPhotosCheckIconHasCorrectPosition', function() {
    assertCheckPosition(AmbientModeTopicSource.GOOGLE_PHOTOS);
  });

  test('setSelectedAlbums', async () => {
    ambientModePhotosPage.albums = [
      {albumId: 'id0', checked: true, title: 'album0', url: 'url'},
      {albumId: 'id1', checked: false, title: 'album1', url: 'url'}
    ];
    Polymer.dom.flush();

    const albumList = ambientModePhotosPage.$$('album-list');
    const ironList = albumList.$$('iron-list');
    const albumItems = ironList.querySelectorAll('album-item:not([hidden])');
    assertEquals(2, albumItems.length);

    const album0 = albumItems[0];
    const album1 = albumItems[1];
    assertTrue(album0.checked);
    assertFalse(album1.checked);

    browserProxy.resetResolver('setSelectedAlbums');

    // Click album item image will toggle the check.
    album1.$$('#image').click();
    assertTrue(album1.checked);

    assertEquals(1, browserProxy.getCallCount('setSelectedAlbums'));
    let albumsArgs = await browserProxy.whenCalled('setSelectedAlbums');
    assertDeepEquals(
        [{albumId: 'id0'}, {albumId: 'id1'}], albumsArgs[0].albums);

    browserProxy.resetResolver('setSelectedAlbums');

    // Click album item image will toggle the check.
    album0.$$('#image').click();
    assertFalse(album0.checked);

    assertEquals(1, browserProxy.getCallCount('setSelectedAlbums'));
    albumsArgs = await browserProxy.whenCalled('setSelectedAlbums');
    assertDeepEquals([{albumId: 'id1'}], albumsArgs[0].albums);
  });

  test('notToggleAlbumSelection', function() {
    ambientModePhotosPage.albums = [
      {albumId: 'id0', checked: true, title: 'album0'},
    ];
    Polymer.dom.flush();

    const albumList = ambientModePhotosPage.$$('album-list');
    const ironList = albumList.$$('iron-list');
    const albumItems = ironList.querySelectorAll('album-item:not([hidden])');
    assertEquals(1, albumItems.length);

    const album0 = albumItems[0];
    assertTrue(album0.checked);

    // Verify that the selected-albums-changed event is sent when the album
    // image is clicked.
    let selectedAlbumsChangedEventCalls = 0;
    albumList.addEventListener('selected-albums-changed', (event) => {
      selectedAlbumsChangedEventCalls++;
    });

    // Click outside album item image will not toggle the check.
    album0.$.albumInfo.click();
    assertTrue(album0.checked);
    assertEquals(0, selectedAlbumsChangedEventCalls);
  });

  test('toggleAlbumSelectionByKeypress', function() {
    ambientModePhotosPage.albums = [
      {albumId: 'id0', checked: true, title: 'album0'},
    ];
    Polymer.dom.flush();

    const albumList = ambientModePhotosPage.$$('album-list');
    const ironList = albumList.$$('iron-list');
    const albumItems = ironList.querySelectorAll('album-item:not([hidden])');
    assertEquals(1, albumItems.length);

    const album0 = albumItems[0];
    assertTrue(album0.checked);

    // Verify that the selected-albums-changed event is sent when the album
    // image is clicked.
    let selectedAlbumsChangedEventCalls = 0;
    albumList.addEventListener('selected-albums-changed', (event) => {
      selectedAlbumsChangedEventCalls++;
    });

    // Keydown with Enter key on album item will toggle the selection.
    const enterEvent = new KeyboardEvent(
        'keydown', {cancelable: true, key: 'Enter', keyCode: 13});
    album0.dispatchEvent(enterEvent);
    assertFalse(album0.checked);
    assertEquals(1, selectedAlbumsChangedEventCalls);

    // Keydown with Enter key on album item will toggle the selection.
    album0.dispatchEvent(enterEvent);
    assertTrue(album0.checked);
    assertEquals(2, selectedAlbumsChangedEventCalls);
  });

  test('updateAlbumURL', function() {
    ambientModePhotosPage.albums = [
      {albumId: 'id0', checked: true, title: 'album0', url: ''},
    ];
    ambientModePhotosPage.topicSource = AmbientModeTopicSource.ART_GALLERY;
    Polymer.dom.flush();

    const albumList = ambientModePhotosPage.$$('album-list');
    const ironList = albumList.$$('iron-list');
    const albumItems = ironList.querySelectorAll('album-item:not([hidden])');
    assertEquals(1, albumItems.length);

    const album0 = albumItems[0];
    assertEquals('', album0.album.url);

    // Update album URL.
    const url = 'chrome://ambient';
    cr.webUIListenerCallback('album-preview-changed', {
      topicSource: AmbientModeTopicSource.ART_GALLERY,
      albumId: 'id0',
      url: url
    });
    assertEquals(url, album0.album.url);
  });

  test('notUpdateAlbumURL', function() {
    ambientModePhotosPage.albums = [
      {albumId: 'id0', checked: true, title: 'album0', url: ''},
    ];
    ambientModePhotosPage.topicSource = AmbientModeTopicSource.ART_GALLERY;
    Polymer.dom.flush();

    const albumList = ambientModePhotosPage.$$('album-list');
    const ironList = albumList.$$('iron-list');
    const albumItems = ironList.querySelectorAll('album-item:not([hidden])');
    assertEquals(1, albumItems.length);

    const album0 = albumItems[0];
    assertEquals('', album0.album.url);

    // Different topic source will no update album URL.
    const url = 'chrome://ambient';
    cr.webUIListenerCallback('album-preview-changed', {
      topicSource: AmbientModeTopicSource.GOOGLE_PHOTOS,
      albumId: 'id0',
      url: url
    });
    assertEquals('', album0.album.url);
  });

  test('updateImgVisibility', function() {
    ambientModePhotosPage.albums = [
      {albumId: 'id0', checked: true, title: 'album0', url: ''},
    ];
    ambientModePhotosPage.topicSource = AmbientModeTopicSource.ART_GALLERY;
    Polymer.dom.flush();

    const albumList = ambientModePhotosPage.$$('album-list');
    const ironList = albumList.$$('iron-list');
    const albumItems = ironList.querySelectorAll('album-item:not([hidden])');
    assertEquals(1, albumItems.length);

    const album0 = albumItems[0];
    assertEquals('', album0.album.url);

    let img = album0.$$('#image');
    assertFalse(!!img);

    // Update album URL.
    const url = 'https://ambient-art-gallery-preview-url';
    cr.webUIListenerCallback('album-preview-changed', {
      topicSource: AmbientModeTopicSource.ART_GALLERY,
      albumId: 'id0',
      url: url
    });
    assertEquals(url, album0.album.url);

    img = album0.$$('#image');
    assertTrue(!!img);
    assertFalse(img.hidden);
  });
});

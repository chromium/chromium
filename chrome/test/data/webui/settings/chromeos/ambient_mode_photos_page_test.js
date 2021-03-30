// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {AmbientModeTopicSource, AmbientModeBrowserProxyImpl} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {TestBrowserProxy} from '../../test_browser_proxy.m.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.m.js';
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
    displayPhotosPage_(
        [
          {albumId: 'id0', checked: true, title: 'album0', url: 'url'},
          {albumId: 'id1', checked: true, title: 'album1', url: 'url'}
        ],
        topicSource);

    getAlbumItems_().forEach((album) => {
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

  /**
   * Retrieve the title element for the albumItem at the position.
   * @param {number} position
   * @return {Element}
   * @private
   */
  function getTitleElement_(position) {
    return getAlbumItems_()[position].$$('#albumTitle');
  }

  /**
   * Retrieve the description element for the albumItem at the position.
   * @param {number} position
   * @return {Element}
   * @private
   */
  function getDescriptionElement_(position) {
    return getAlbumItems_()[position].$$('#albumDescription');
  }

  /**
   * Setup the photos page and flush the DOM.
   * @param {Array<!AmbientModeAlbum>} albums
   * @param {!AmbientModeTopicSource} topicSource
   * @private
   */
  function displayPhotosPage_(albums, topicSource) {
    ambientModePhotosPage.albums = albums;
    ambientModePhotosPage.topicSource = topicSource;
    Polymer.dom.flush();
  }

  /**
   * Get all displayed album item elements.
   * @return {Array<Element>}
   * @private
   */
  function getAlbumItems_() {
    const albumList = ambientModePhotosPage.$$('album-list');
    const ironList = albumList.$$('iron-list');
    return ironList.querySelectorAll('album-item:not([hidden])');
  }

  /**
   * @param {Element} element
   * @return {number}
   * @private
   */
  function getZIndex_(element) {
    const zIndex = parseInt(getComputedStyle(element).zIndex);
    return zIndex === NaN ? 0 : zIndex;
  }

  /**
   * Asserts the title and the description elements are limited to 1 and 2 lines
   * respectively.
   * @param {number} albumIndex
   * @private
   */
  function assertAllTextClamped_(albumIndex) {
    assertTextClamped(getTitleElement_(albumIndex), 1);
    assertTextClamped(getDescriptionElement_(albumIndex), 2);
  }

  /**
   * Asserts that the textWithTooltip element is limited to the specified number
   * of lines.
   * @param {Element} textWithTooltip
   * @param {number} lineCount
   * @private
   */
  function assertTextClamped(textWithTooltip, lineCount) {
    const element = textWithTooltip.$$('#textDiv');
    const height = element.offsetHeight;
    const lineHeight = parseInt(getComputedStyle(element).lineHeight);
    assertTrue(
        height / lineHeight <= lineCount,
        'Actual Height: ' + height.toString() + ' Line height:  ' +
            lineHeight.toString() + ' Content: ' + element.innerHTML);
  }

  /**
   * Generate a random UTF16 String of specified length.
   * @param {number} length The length of the string
   * @return {String}
   * @private
   */
  function createRandomString_(length) {
    let randomString = '';
    for (let i = 0; i < length; i++) {
      randomString = randomString.concat(getRandomUTF16Char_());
    }
    return randomString;
  }

  /**
   * Generate a random UTF16 character.
   * @return {String} The character
   * @private
   */
  function getRandomUTF16Char_() {
    const utf16Max = 65535;
    return String.fromCharCode(Math.floor(Math.random() * utf16Max));
  }

  /**
   * Determines if a tooltip is visible for the parameter.
   * @param {Element} parentElement The element containing the tooltip.
   * @return {boolean} If a tooltip element is within an animate in delay or is
   * already visible/animating.
   */
  function isTooltipAvailable_(parentElement) {
    const tooltip = parentElement.$$('paper-tooltip');
    return tooltip !== null && getComputedStyle(tooltip).display !== 'none';
  }

  test('hasAlbumsWithoutPhotoPreview', function() {
    // Disable photo preview feature and reload the |ambientModePhotosPage|.
    loadTimeData.overrideValues({isAmbientModePhotoPreviewEnabled: false});
    assertFalse(loadTimeData.getBoolean('isAmbientModePhotoPreviewEnabled'));

    ambientModePhotosPage.remove();
    ambientModePhotosPage =
        document.createElement('settings-ambient-mode-photos-page');
    document.body.appendChild(ambientModePhotosPage);

    displayPhotosPage_(
        [
          {albumId: 'id0', checked: true, title: 'album0'},
          {albumId: 'id1', checked: false, title: 'album1'}
        ],
        null);

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
    displayPhotosPage_(
        [
          {albumId: 'id0', checked: true, title: 'album0'},
          {albumId: 'id1', checked: false, title: 'album1'}
        ],
        null);

    const albumItems = getAlbumItems_();
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

  test('spinnerVisibility', function() {
    const albumList = ambientModePhotosPage.$$('album-list');
    const spinner = albumList.$$('paper-spinner-lite');
    assertTrue(!!spinner);
    assertTrue(spinner.active);
    assertFalse(spinner.hidden);

    displayPhotosPage_(
        [
          {albumId: 'id0', checked: true, title: 'album0'},
          {albumId: 'id1', checked: false, title: 'album1'}
        ],
        null);

    // Spinner is not active and not visible.
    assertFalse(spinner.active);
    assertTrue(spinner.hidden);
  });

  test('personalPhotosImageContainerHasCorrectSize', function() {
    displayPhotosPage_(
        [
          {albumId: 'id0', checked: true, title: 'album0'},
          {albumId: 'id1', checked: false, title: 'album1'},
          {albumId: 'id2', checked: false, title: 'album2'}
        ],
        AmbientModeTopicSource.GOOGLE_PHOTOS);

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
    displayPhotosPage_(
        [
          {albumId: 'id0', checked: true, title: 'album0'},
          {albumId: 'id1', checked: false, title: 'album1'},
          {albumId: 'id2', checked: false, title: 'album2'}
        ],
        AmbientModeTopicSource.ART_GALLERY);

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
    displayPhotosPage_(
        [
          {albumId: 'id0', checked: true, title: 'album0', url: 'url'},
          {albumId: 'id1', checked: false, title: 'album1', url: 'url'}
        ],
        null);

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

  test('notDeselectLastArtAlbum', async () => {
    displayPhotosPage_(
        [
          {albumId: 'id0', checked: true, title: 'album0', url: 'url'},
          {albumId: 'id1', checked: true, title: 'album1', url: 'url'}
        ],
        AmbientModeTopicSource.ART_GALLERY);

    const albumItems = getAlbumItems_();
    assertEquals(2, albumItems.length);

    const album0 = albumItems[0];
    const album1 = albumItems[1];
    assertTrue(album0.checked);
    assertTrue(album1.checked);

    // Click album item image will toggle the check.
    const image0 = album0.$$('#image');
    image0.click();
    assertFalse(album0.checked);

    // Click the last art album item image will not toggle the check and will
    // show a dialog.
    const image1 = album1.$$('#image');
    image1.click();
    assertTrue(album1.checked);
    Polymer.dom.flush();

    const artAlbumDialog = ambientModePhotosPage.$$('art-album-dialog');
    await test_util.waitAfterNextRender(artAlbumDialog);
    assertTrue(artAlbumDialog.$$('#dialog').open);
  });

  test('showCheckIconOnSelectedAlbum', function() {
    displayPhotosPage_(
        [
          {albumId: 'id0', checked: true, title: 'album0', url: 'url'},
          {albumId: 'id1', checked: false, title: 'album1', url: 'url'}
        ],
        null);

    const album0 = getAlbumItems_()[0];
    const check0 = album0.$$('.check');
    assertTrue(album0.checked);
    assertFalse(check0.hidden);

    // Click album item image will toggle the check.
    album0.$$('#image').click();
    assertFalse(album0.checked);
    assertTrue(check0.hidden);

    const album1 = getAlbumItems_()[1];
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
    assertCheckPosition(AmbientModeTopicSource.ART_GALLERY);
  });

  test('setSelectedAlbums', async () => {
    displayPhotosPage_(
        [
          {albumId: 'id0', checked: true, title: 'album0', url: 'url'},
          {albumId: 'id1', checked: false, title: 'album1', url: 'url'}
        ],
        null);

    const albumItems = getAlbumItems_();
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
    displayPhotosPage_(
        [
          {albumId: 'id0', checked: true, title: 'album0'},
        ],
        null);

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
    displayPhotosPage_(
        [
          {albumId: 'id0', checked: true, title: 'album0'},
        ],
        null);

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
    displayPhotosPage_(
        [
          {albumId: 'id0', checked: true, title: 'album0'},
        ],
        AmbientModeTopicSource.ART_GALLERY);

    const albumItems = getAlbumItems_();
    assertEquals(1, albumItems.length);

    const album0 = albumItems[0];

    // Update album URL.
    const url = 'url';
    cr.webUIListenerCallback('album-preview-changed', {
      topicSource: AmbientModeTopicSource.ART_GALLERY,
      albumId: 'id0',
      url: url
    });
    assertEquals(url, album0.album.url);
  });

  test('notUpdateAlbumURL', function() {
    displayPhotosPage_(
        [
          {albumId: 'id0', checked: true, title: 'album0'},
        ],
        AmbientModeTopicSource.ART_GALLERY);

    const albumItems = getAlbumItems_();
    assertEquals(1, albumItems.length);

    const album0 = albumItems[0];

    // Different topic source will no update album URL.
    const url = 'chrome://ambient';
    cr.webUIListenerCallback('album-preview-changed', {
      topicSource: AmbientModeTopicSource.GOOGLE_PHOTOS,
      albumId: 'id0',
      url: url
    });
    assertFalse(!!album0.album.url);
  });

  test('updateImgVisibility', function() {
    displayPhotosPage_(
        [
          {albumId: 'id0', checked: true, title: 'album0'},
        ],
        AmbientModeTopicSource.ART_GALLERY);

    const albumItems = getAlbumItems_();
    assertEquals(1, albumItems.length);

    const album0 = albumItems[0];

    let img = album0.$$('#image');
    assertFalse(!!img);

    // Update album URL.
    const url = 'url';
    cr.webUIListenerCallback('album-preview-changed', {
      topicSource: AmbientModeTopicSource.ART_GALLERY,
      albumId: 'id0',
      url: url
    });
    assertEquals(url, album0.album.url);

    img = album0.$$('#image');
    assertTrue(!!img);
    assertFalse(img.hidden);

    const images = album0.$$('#rhImages');
    assertFalse(!!images);
  });

  test('updateRecentHighlightsImagesVisibility', function() {
    displayPhotosPage_(
        [
          {albumId: 'id0', checked: true, title: 'album0'},
        ],
        AmbientModeTopicSource.GOOGLE_PHOTOS);

    const albumItems = getAlbumItems_();
    assertEquals(1, albumItems.length);

    const album0 = albumItems[0];
    let images = album0.$$('#rhImages');
    assertFalse(!!images);

    // Update Recent Highlights album URLs.
    const url = 'url';
    cr.webUIListenerCallback('album-preview-changed', {
      topicSource: AmbientModeTopicSource.GOOGLE_PHOTOS,
      albumId: 'id0',
      recentHighlightsUrls: [url, url, url, url]
    });
    assertEquals(url, album0.album.recentHighlightsUrls[0]);
    assertEquals(url, album0.album.recentHighlightsUrls[1]);
    assertEquals(url, album0.album.recentHighlightsUrls[2]);
    assertEquals(url, album0.album.recentHighlightsUrls[3]);
    images = album0.$$('#rhImages');
    assertTrue(!!images);
    assertFalse(images.hidden);
    const image_top_left = album0.$$('.image-rh.top-left');
    const image_top_right = album0.$$('.image-rh.top-right');
    const image_bottom_left = album0.$$('.image-rh.bottom-left');
    const image_bottom_right = album0.$$('.image-rh.bottom-right');
    assertTrue(!!image_top_left);
    assertFalse(image_top_left.hidden);
    assertTrue(!!image_top_right);
    assertFalse(image_top_right.hidden);
    assertTrue(!!image_bottom_left);
    assertFalse(image_bottom_left.hidden);
    assertTrue(!!image_bottom_right);
    assertFalse(image_bottom_right.hidden);

    const img = album0.$$('#image');
    assertFalse(!!img);
  });

  test('updateRecentHighlightsImagesVisibilityWithThreeImages', function() {
    displayPhotosPage_(
        [
          {albumId: 'id0', checked: true, title: 'album0'},
        ],
        AmbientModeTopicSource.GOOGLE_PHOTOS);

    const albumItems = getAlbumItems_();
    assertEquals(1, albumItems.length);

    const album0 = albumItems[0];
    let images = album0.$$('#rhImages');
    assertFalse(!!images);

    // Only update 3 images.
    const url = 'url';
    cr.webUIListenerCallback('album-preview-changed', {
      topicSource: AmbientModeTopicSource.GOOGLE_PHOTOS,
      albumId: 'id0',
      recentHighlightsUrls: [url, url, url]
    });
    assertEquals(url, album0.album.recentHighlightsUrls[0]);
    assertEquals(url, album0.album.recentHighlightsUrls[1]);
    assertEquals(url, album0.album.recentHighlightsUrls[2]);
    assertFalse(!!album0.album.recentHighlightsUrls[3]);
    images = album0.$$('#rhImages');
    assertTrue(!!images);
    assertFalse(images.hidden);
    const image_top_left = album0.$$('.image-rh.top-left');
    const image_top_right = album0.$$('.image-rh.top-right');
    const image_bottom_left = album0.$$('.image-rh.bottom-left');
    const image_bottom_right = album0.$$('.image-rh.bottom-right');
    assertTrue(!!image_top_left);
    assertFalse(image_top_left.hidden);
    assertTrue(!!image_top_right);
    assertFalse(image_top_right.hidden);
    assertTrue(!!image_bottom_left);
    assertFalse(image_bottom_left.hidden);
    assertTrue(!!image_bottom_right);
    assertTrue(image_bottom_right.hidden);

    const img = album0.$$('#image');
    assertFalse(!!img);
  });

  test('albumSizeIsConsistent', function() {
    displayPhotosPage_(
        [
          {
            albumId: 'id0',
            checked: false,
            title: createRandomString_(1),
            description: createRandomString_(1),
            url: 'url'
          },
          {
            albumId: 'id1',
            checked: false,
            title: createRandomString_(500),
            description: createRandomString_(500),
            url: 'url'
          },
        ],
        AmbientModeTopicSource.GOOGLE_PHOTOS);

    const album0 = getAlbumItems_()[0];
    const album1 = getAlbumItems_()[1];
    assertEquals(
        album0.offsetHeight, album1.offsetHeight,
        'grids in iron-list require height to be consistent across items');
  });

  test('linesAreClamped', function() {
    displayPhotosPage_(
        [
          {
            albumId: 'id0',
            checked: false,
            title: createRandomString_(1),
            description: createRandomString_(1),
            url: 'url'
          },
          {
            albumId: 'id1',
            checked: false,
            title: createRandomString_(500),
            description: createRandomString_(500),
            url: 'url'
          },
        ],
        AmbientModeTopicSource.GOOGLE_PHOTOS);

    assertAllTextClamped_(0);
    assertAllTextClamped_(1);
  });

  test('tooltipVisibilityAdjustsZIndex', function() {
    displayPhotosPage_(
        [
          {
            albumId: 'id0',
            checked: false,
            title: createRandomString_(500),
            description: createRandomString_(500),
            url: 'url'
          },
          {
            albumId: 'id1',
            checked: false,
            title: createRandomString_(500),
            description: createRandomString_(500),
            url: 'url'
          },
        ],
        AmbientModeTopicSource.GOOGLE_PHOTOS);
    const album0 = getAlbumItems_()[0];
    const album1 = getAlbumItems_()[1];

    album0.titleTooltipIsVisible = true;
    assertTrue(getZIndex_(album0) > getZIndex_(album1));

    album0.titleTooltipIsVisible = false;
    album1.titleTooltipIsVisible = true;
    assertTrue(getZIndex_(album0) < getZIndex_(album1));
    album1.titleTooltipIsVisible = false;

    album1.descriptionTooltipIsVisible = true;
    assertTrue(getZIndex_(album0) < getZIndex_(album1));

    album0.descriptionTooltipIsVisible = true;
    album1.descriptionTooltipIsVisible = false;
    assertTrue(getZIndex_(album0) > getZIndex_(album1));
  });

  test('textWithTooltip_hasTooltipWhenTextOverflows', function() {
    displayPhotosPage_(
        [
          {
            albumId: 'id0',
            checked: false,
            title: createRandomString_(1),
            description: createRandomString_(1),
            url: 'url'
          },
          {
            albumId: 'id1',
            checked: false,
            title: createRandomString_(500),
            description: createRandomString_(500),
            url: 'url'
          },
        ],
        AmbientModeTopicSource.GOOGLE_PHOTOS);
    assertFalse(
        isTooltipAvailable_(getTitleElement_(0)),
        'There shouldn\'t be a tooltip for non-overflowing text.');
    assertFalse(
        isTooltipAvailable_(getDescriptionElement_(0)),
        'There shouldn\'t be a tooltip for non-overflowing text.');

    assertTrue(
        isTooltipAvailable_(getTitleElement_(1)),
        'There should be a tooltip for overflowing text.');
    assertTrue(
        isTooltipAvailable_(getDescriptionElement_(1)),
        'There should be a tooltip for overflowing text.');
  });

  test('textWithTooltip_tooltipVisibilityWhenTextChanges', function() {
    displayPhotosPage_(
        [
          {
            albumId: 'id0',
            checked: false,
            title: createRandomString_(1),
            description: createRandomString_(1),
            url: 'url'
          },
        ],
        AmbientModeTopicSource.GOOGLE_PHOTOS);

    assertFalse(
        isTooltipAvailable_(getTitleElement_(0)),
        'There shouldn\'t be a tooltip for non-overflowing text.');

    getTitleElement_(0).text = createRandomString_(300);
    Polymer.dom.flush();
    assertTrue(
        isTooltipAvailable_(getTitleElement_(0)),
        'There should be a tooltip for overflowing text.');

    getTitleElement_(0).text = createRandomString_(1);
    Polymer.dom.flush();
    assertFalse(
        isTooltipAvailable_(getTitleElement_(0)),
        'There shouldn\'t be a tooltip for non-overflowing text.');
  });

  test('textWithTooltip_tooltipVisibilityCallbacks', function() {
    displayPhotosPage_(
        [
          {
            albumId: 'id0',
            checked: false,
            title: createRandomString_(500),
            description: createRandomString_(1),
            url: 'url'
          },
        ],
        AmbientModeTopicSource.GOOGLE_PHOTOS);
    getTitleElement_(0).dispatchEvent(new MouseEvent('mouseenter', null));
    assertFalse(
        getTitleElement_(0).tooltipIsVisible,
        'Tooltip should have an animate in delay');
    // Delay animate in delay and animate out duration is currently 500.
    setTimeout(() => {
      assertTrue(
          getTitleElement_(0).tooltipIsVisible, 'Tooltip should be visible');

      getTitleElement_(0).dispatchEvent(new MouseEvent('mouseleave', null));
      assertTrue(
          getTitleElement_(0).tooltipIsVisible,
          'Tooltip should have an animate out duration');
      setTimeout(() => {
        assertFalse(
            getTitleElement_(0).tooltipIsVisible,
            'Tooltip shouldn\'t be visible after it\'s finished animating out');
      }, 1000);
    }, 1000);
  });
});

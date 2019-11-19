// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests local NTP custom backgrounds and the original background
 * customization menu.
 */

/**
 * Local NTP's object for test and setup functions.
 */
test.customBackgrounds = {};

/**
 * Sets up the page for each individual test.
 */
test.customBackgrounds.setUp = function() {
  setUpPage('local-ntp-template');
};

// ******************************* SIMPLE TESTS *******************************
// These are run by runSimpleTests above.
// Functions from test_utils.js are automatically imported.

/**
 * Tests that the edit custom background button is visible if both the flag is
 * enabled and no custom theme is being used.
 */
test.customBackgrounds.testShowEditCustomBackground = function() {
  initLocalNTP(/*isGooglePage=*/true);

  assertTrue(elementIsVisible($('edit-bg')));
};

/**
 * Tests that clicking on the gear icon opens the background option dialog.
 */
test.customBackgrounds.testClickGearIcon = function() {
  initLocalNTP(/*isGooglePage=*/true);

  $('edit-bg').click();

  assertTrue(elementIsVisible($('edit-bg-dialog')));
};

/**
 * Test that clicking on the "Chrome backgrounds" option results in a correct
 * selection dialog.
 */
test.customBackgrounds.testClickChromeBackgrounds = function() {
  initLocalNTP(/*isGooglePage=*/true);

  $('edit-bg').click();
  setupFakeAsyncCollectionLoad();
  $('edit-bg-default-wallpapers').click();

  checkCollectionDialog();
};

/**
 * Test that clicking the cancel button on the collection selection dialog
 * closes the dialog.
 */
test.customBackgrounds.testCollectionDialogCancel = function() {
  initLocalNTP(/*isGooglePage=*/true);

  $('edit-bg').click();
  setupFakeAsyncCollectionLoad();
  $('edit-bg-default-wallpapers').click();
  $('bg-sel-footer-cancel').click();

  assertFalse(elementIsVisible($('bg-sel-menu')));
};

/**
 * Test that clicking the done button on the collection selection dialog does
 * nothing.
 */
test.customBackgrounds.testCollectionDialogDone = function() {
  initLocalNTP(/*isGooglePage=*/true);

  $('edit-bg').click();
  setupFakeAsyncCollectionLoad();
  $('edit-bg-default-wallpapers').click();
  $('bg-sel-footer-done').click();

  checkCollectionDialog();
};

/**
 * Test that clicking on a collection tile opens and loads the image selection
 * dialog.
 */
test.customBackgrounds.testCollectionDialogTileClick = function() {
  initLocalNTP(/*isGooglePage=*/true);

  $('edit-bg').click();
  setupFakeAsyncCollectionLoad();
  $('edit-bg-default-wallpapers').click();
  setupFakeAsyncImageLoad('coll_tile_0');
  var event = new Event('click', {'target': $('coll_tile_0')});
  $('coll_tile_0').click(event);

  checkImageDialog();
};

/**
 * Test that clicking cancel on the image selection dialog closes the dialog.
 */
test.customBackgrounds.testImageDialogCancel = function() {
  initLocalNTP(/*isGooglePage=*/true);

  $('edit-bg').click();
  setupFakeAsyncCollectionLoad();
  $('edit-bg-default-wallpapers').click();
  setupFakeAsyncImageLoad('coll_tile_0');
  var event = new Event('click', {'target': $('coll_tile_0')});
  $('coll_tile_0').click(event);
  $('bg-sel-footer-cancel').click();

  assertFalse(elementIsVisible($('bg-sel-menu')));
};

/**
 * Test that clicking the back button on the image selection dialog results in
 * the collection selection dialog being displayed.
 */
test.customBackgrounds.testImageDialogBack = function() {
  initLocalNTP(/*isGooglePage=*/true);

  $('edit-bg').click();
  setupFakeAsyncCollectionLoad();
  $('edit-bg-default-wallpapers').click();
  setupFakeAsyncImageLoad('coll_tile_0');
  var event = new Event('click', {'target': $('coll_tile_0')});
  $('coll_tile_0').click(event);
  $('bg-sel-back').click();

  checkCollectionDialog();
};

/**
 * Test that clicking on an image tile applies the selected styling.
 */
test.customBackgrounds.testImageTileClick = function() {
  initLocalNTP(/*isGooglePage=*/true);

  $('edit-bg').click();
  setupFakeAsyncCollectionLoad();
  $('edit-bg-default-wallpapers').click();
  setupFakeAsyncImageLoad('coll_tile_0');
  var event = new Event('click', {'target': $('coll_tile_0')});
  $('coll_tile_0').click(event);
  $('img_tile_0').click();

  assertTrue($('img_tile_0').classList.contains('bg-selected'));
};

/**
 * Test that clicking done with no image selected does nothing.
 */
test.customBackgrounds.testImageDoneClickNoneSelected = function() {
  initLocalNTP(/*isGooglePage=*/true);

  $('edit-bg').click();
  setupFakeAsyncCollectionLoad();
  $('edit-bg-default-wallpapers').click();
  setupFakeAsyncImageLoad('coll_tile_0');
  var event = new Event('click', {'target': $('coll_tile_0')});
  $('coll_tile_0').click(event);
  $('bg-sel-footer-done').click();

  checkImageDialog();
};

/**
 * Test that clicking done with an image selected closes the dialog.
 */
test.customBackgrounds.testImageDoneClick = function() {
  initLocalNTP(/*isGooglePage=*/true);

  $('edit-bg').click();
  setupFakeAsyncCollectionLoad();
  $('edit-bg-default-wallpapers').click();
  setupFakeAsyncImageLoad('coll_tile_0');
  var event = new Event('click', {'target': $('coll_tile_0')});
  $('coll_tile_0').click(event);
  $('img_tile_0').click();
  $('bg-sel-footer-done').click();

  assertFalse(elementIsVisible($('bg-sel-menu')));
};

/**
 * Test that no custom background option will be shown when offline.
 */
test.customBackgrounds.testHideCustomBackgroundOffline = function() {
  initLocalNTP(/*isGooglePage=*/true);

  let event = new Event('offline', {});
  window.dispatchEvent(event);
  $('edit-bg').click();

  assertFalse(elementIsVisible($('edit-bg-default-wallpapers-text')));
};

/**
 * Test that clicking collection when offline will trigger an error
 * notification.
 */
test.customBackgrounds.testClickCollectionOfflineShowErrorMsg = function() {
  initLocalNTP(/*isGooglePage=*/true);

  $('edit-bg').click();
  setupFakeAsyncCollectionLoad();
  $('edit-bg-default-wallpapers').click();
  setupFakeAsyncImageLoadOffline('coll_tile_0');
  let event = new Event('click', {'target': $('coll_tile_0')});
  $('coll_tile_0').click(event);

  assertTrue(elementIsVisible($('error-notice')));
};

// TODO(crbug.com/857256): add tests for:
//  * Image upload flow.
//  * Online/offline.

// ******************************* HELPERS *******************************

/**
 * Fake the loading of the Chrome Backgrounds collections so it happens
 * synchronously.
 */
setupFakeAsyncCollectionLoad = function() {
  // Override the collection loading script.
  customize.loadChromeBackgrounds = function() {
    var collScript = document.createElement('script');
    collScript.id = 'ntp-collection-loader';
    document.body.appendChild(collScript);
    coll = [
      {
        collectionId: 'collection1',
        collectionName: 'Collection 1',
        previewImageUrl: 'chrome-search://local-ntp/background.jpg'
      },
      {
        collectionId: 'collection2',
        collectionName: 'Collection 2',
        previewImageUrl: 'chrome-search://local-ntp/background.jpg'
      },
      {
        collectionId: 'collection3',
        collectionName: 'Collection 3',
        previewImageUrl: 'chrome-search://local-ntp/background.jpg'
      }
    ];
    collErrors = {};
  };

  // Append a call to onload to the end of the click handler.
  var oldBackgroundsFunc = $('edit-bg-default-wallpapers').onclick;
  $('edit-bg-default-wallpapers').onclick = function() {
    oldBackgroundsFunc();
    $('ntp-collection-loader').onload();
  }
};

/**
 * Fake the loading of the a collection's images so it happens synchronously.
 */
setupFakeAsyncImageLoad = function(tile_id) {

  // Append the creation of the image data and a call to onload to the
  // end of the click handler.
  var oldImageLoader = $(tile_id).onclick;
  $(tile_id).onclick = function(event) {
    oldImageLoader(event);
    collImg = [
      {
        attributionActionUrl: 'https://www.google.com',
        attributions: ['test1', 'attribution1'],
        collectionId: 'collection1',
        imageUrl: 'chrome-search://local-ntp/background1.jpg',
        thumbnailImageUrl: 'chrome-search://local-ntp/background_thumbnail.jpg1'
      },
      {
        attributionActionUrl: 'https://www.google.com',
        attributions: ['test2', 'attribution2'],
        collectionId: 'collection1',
        imageUrl: 'chrome-search://local-ntp/background2.jpg',
        thumbnailImageUrl: 'chrome-search://local-ntp/background_thumbnail.jpg2'
      },
      {
        attributionActionUrl: 'https://www.google.com',
        attributions: ['test3', 'attribution3'],
        collectionId: 'collection1',
        imageUrl: 'chrome-search://local-ntp/background3.jpg',
        thumbnailImageUrl: 'chrome-search://local-ntp/background_thumbnail.jpg3'
      },
      {
        attributionActionUrl: 'https://www.google.com',
        attributions: ['test4', 'attribution4'],
        collectionId: 'collection1',
        imageUrl: 'chrome-search://local-ntp/background4.jpg',
        thumbnailImageUrl: 'chrome-search://local-ntp/background_thumbnail.jpg4'
      },
      {
        attributionActionUrl: 'https://www.google.com',
        attributions: ['test5', 'attribution5'],
        collectionId: 'collection1',
        imageUrl: 'chrome-search://local-ntp/background5.jpg',
        thumbnailImageUrl: 'chrome-search://local-ntp/background_thumbnail.jpg5'
      }
    ];
    collImgErrors = {};
    $('ntp-images-loader').onload();
  }
};

/**
 * Fake loading a collection's images with a network error to simulate offline
 * status.
 */
setupFakeAsyncImageLoadOffline = function(tile_id) {
  // Override the image tile's onclick function.
  let oldImageLoader = $(tile_id).onclick;
  $(tile_id).onclick = function(event) {
    oldImageLoader(event);
    collImg = [];
    collImgErrors = {net_error: true, net_error_no: -106};
    $('ntp-images-loader').onload();
  }
};

/**
 * Check that the collection selection dialog contains the correct elements.
 */
checkCollectionDialog = function() {
  assertTrue(elementIsVisible($('bg-sel-menu')));
  assertTrue($('bg-sel-menu').classList.contains('is-col-sel'));
  assertTrue(
      $('bg-sel-menu').getElementsByClassName('bg-sel-tile').length ==
      coll.length);
  assertTrue(
      $('bg-sel-menu').getElementsByClassName('bg-sel-tile-title').length ==
      coll.length);
  assertFalse(elementIsVisible($('bg-sel-back')));
  assertTrue(elementIsVisible($('bg-sel-footer-cancel')));
  assertTrue(elementIsVisible($('bg-sel-footer-done')));
};

/**
 * Check that the image selection dialog contains the correct elements.
 */
checkImageDialog = function() {
  assertTrue(elementIsVisible($('bg-sel-menu')));
  assertTrue($('bg-sel-menu').classList.contains('is-img-sel'));
  assertTrue(
      $('bg-sel-menu').getElementsByClassName('bg-sel-tile').length ==
      collImg.length);
  assertTrue(elementIsVisible($('bg-sel-back')));
  assertTrue(elementIsVisible($('bg-sel-footer-cancel')));
  assertTrue(elementIsVisible($('bg-sel-footer-done')));
};

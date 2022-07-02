// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ChangePictureBrowserProxyImpl, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {CrPicture} from 'chrome://resources/cr_elements/chromeos/cr_picture/cr_picture_types.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {TestBrowserProxy} from '../../test_browser_proxy.js';

/** @implements {ChangePictureBrowserProxy} */
class TestChangePictureBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'initialize',
      'selectDefaultImage',
      'selectOldImage',
      'selectProfileImage',
      'photoTaken',
      'chooseFile',
      'requestSelectedImage',
    ]);
  }

  /** @override */
  initialize() {
    webUIListenerCallback(
        'profile-image-changed', 'fake-profile-image-url',
        false /* selected */);

    const fakeCurrentDefaultImages = [
      {
        index: 2,
        title: 'Title2',
        url: 'chrome://foo/2.png',
      },
      {
        index: 3,
        title: 'Title3',
        url: 'chrome://foo/3.png',
      },
    ];
    webUIListenerCallback('default-images-changed', {
      current_default_images: fakeCurrentDefaultImages,
    });

    this.methodCalled('initialize');
  }

  /** @override */
  selectDefaultImage(imageUrl) {
    webUIListenerCallback('selected-image-changed', imageUrl);
    this.methodCalled('selectDefaultImage', imageUrl);
  }

  /** @override */
  selectOldImage() {
    webUIListenerCallback('old-image-changed', {
      url: 'fake-old-image.jpg',
      index: 1,
    });
    this.methodCalled('selectOldImage');
  }

  /** @override */
  selectProfileImage() {
    webUIListenerCallback(
        'profile-image-changed', 'fake-profile-image-url', true /* selected */);
    this.methodCalled('selectProfileImage');
  }

  /** @override */
  photoTaken() {
    this.methodCalled('photoTaken');
  }

  /** @override */
  chooseFile() {
    this.methodCalled('chooseFile');
  }

  /** @override */
  requestSelectedImage() {
    this.methodCalled('requestSelectedImage');
  }
}

suite('ChangePictureTests', function() {
  let changePicture = null;
  let browserProxy = null;
  let crPicturePane = null;
  let crPictureList = null;

  const LEFT_KEY_CODE = 37;
  const RIGHT_KEY_CODE = 39;

  /**
   * @return {Array<HTMLElement>} Traverses the DOM tree to find the lowest
   *     level active element and returns an array of the node path down the
   *     tree, skipping shadow roots.
   */
  function getActiveElementPath() {
    let node = document.activeElement;
    const path = [];
    while (node) {
      path.push(node);
      node = (node.shadowRoot || node).activeElement;
    }
    return path;
  }


  suiteSetup(function() {
    loadTimeData.overrideValues({
      profilePhoto: 'Fake Profile Photo description',
    });
  });

  setup(async function() {
    browserProxy = new TestChangePictureBrowserProxy();
    ChangePictureBrowserProxyImpl.setInstanceForTesting(browserProxy);
    PolymerTest.clearBody();
    changePicture = document.createElement('settings-change-picture');
    document.body.appendChild(changePicture);

    crPicturePane = changePicture.shadowRoot.querySelector('cr-picture-pane');
    assertTrue(!!crPicturePane);

    crPictureList = changePicture.shadowRoot.querySelector('cr-picture-list');
    assertTrue(!!crPictureList);

    changePicture.currentRouteChanged(routes.CHANGE_PICTURE);

    await browserProxy.whenCalled('initialize');
    flush();
  });

  teardown(function() {
    changePicture.remove();
  });

  test('TraverseCameraIconUsingArrows', function() {
    // Force the camera to be present.
    webUIListenerCallback('camera-presence-changed', true);
    flush();
    assertTrue(crPictureList.cameraPresent);

    // Click camera icon.
    const cameraImage = crPictureList.$.cameraImage;
    cameraImage.click();
    flush();

    assertTrue(crPictureList.cameraSelected_);
    const crCamera = crPicturePane.shadowRoot.querySelector('#camera');
    assertTrue(!!crCamera);

    // Mock camera's video stream beginning to play.
    crCamera.$.cameraVideo.dispatchEvent(new Event('canplay'));
    flush();

    // "Take photo" button should be active.
    let activeElementPath = getActiveElementPath();
    assertTrue(activeElementPath.includes(crPicturePane));
    assertFalse(activeElementPath.includes(crPictureList));

    // Press 'Right' key on active element.
    pressAndReleaseKeyOn(activeElementPath.pop(), RIGHT_KEY_CODE);
    flush();

    // A profile picture open should be active.
    activeElementPath = getActiveElementPath();
    assertFalse(crPictureList.cameraSelected_);
    assertFalse(activeElementPath.includes(crPicturePane));
    assertTrue(activeElementPath.includes(crPictureList));

    // Press 'Left' key on active element.
    pressAndReleaseKeyOn(activeElementPath.pop(), LEFT_KEY_CODE);
    flush();

    // Mock camera's video stream beginning to play.
    crCamera.$.cameraVideo.dispatchEvent(new Event('canplay'));
    flush();

    // "Take photo" button should be active again.
    activeElementPath = getActiveElementPath();
    assertTrue(crPictureList.cameraSelected_);
    assertTrue(activeElementPath.includes(crPicturePane));
    assertFalse(activeElementPath.includes(crPictureList));
  });

  test('ChangePictureSelectCamera', async function() {
    // Force the camera to be absent, even if it's actually present.
    webUIListenerCallback('camera-presence-changed', false);
    flush();

    await new Promise(function(resolve) {
      changePicture.async(resolve);
    });
    let camera = crPicturePane.shadowRoot.querySelector('#camera');
    assertFalse(crPicturePane.cameraPresent);
    assertFalse(crPicturePane.cameraActive_);
    assertFalse(!!camera && camera.hidden);

    webUIListenerCallback('camera-presence-changed', true);
    flush();
    await new Promise(function(resolve) {
      changePicture.async(resolve);
    });
    camera = crPicturePane.shadowRoot.querySelector('#camera');
    assertTrue(crPicturePane.cameraPresent);
    assertFalse(crPicturePane.cameraActive_);
    assertFalse(!!camera && camera.hidden);

    const cameraImage = crPictureList.$.cameraImage;
    cameraImage.click();
    flush();
    await new Promise(function(resolve) {
      changePicture.async(resolve);
    });
    camera = crPicturePane.shadowRoot.querySelector('#camera');
    assertTrue(crPicturePane.cameraActive_);
    assertTrue(!!camera && !camera.hidden);
    assertEquals(
        CrPicture.SelectionTypes.CAMERA,
        changePicture.selectedItem_.dataset.type);
    const discard = crPicturePane.shadowRoot.querySelector('#discard');
    assertTrue(!discard || discard.hidden);

    // Ensure that the camera is deactivated if user navigates away.
    changePicture.currentRouteChanged(routes.BASIC);
    await new Promise(function(resolve) {
      changePicture.async(resolve);
    });
    assertFalse(crPicturePane.cameraActive_);
  });

  test('ChangePictureProfileImage', async function() {
    const profileImage = crPictureList.$.profileImage;
    assertTrue(!!profileImage);

    assertEquals(null, changePicture.selectedItem_);
    profileImage.click();

    await browserProxy.whenCalled('selectProfileImage');
    flush();

    assertEquals(
        CrPicture.SelectionTypes.PROFILE,
        changePicture.selectedItem_.dataset.type);
    assertFalse(crPicturePane.cameraActive_);
    const discard = crPicturePane.shadowRoot.querySelector('#discard');
    assertTrue(!discard || discard.hidden);

    // Ensure that the selection is restored after navigating away and
    // then back to the subpage.
    changePicture.currentRouteChanged(routes.BASIC);
    changePicture.currentRouteChanged(routes.CHANGE_PICTURE);
    assertEquals(null, changePicture.selectedItem_);
  });

  test('ChangePictureDeprecatedImage', async function() {
    webUIListenerCallback(
        'preview-deprecated-image', {url: 'fake-old-image.jpg'});
    flush();

    // Expect the deprecated image is presented in picture pane.
    assertEquals(CrPicture.SelectionTypes.DEPRECATED, crPicturePane.imageType);
    const image = crPicturePane.shadowRoot.querySelector('#image');
    assertTrue(!!image);
    assertFalse(image.hidden);
    const discard = crPicturePane.shadowRoot.querySelector('#discard');
    assertTrue(!!discard);
    assertTrue(discard.hidden);
  });

  test('ChangePictureDeprecatedImageWithSourceInfo', async function() {
    const fakeAuthor = 'FakeAuthor';
    const fakeWebsite = 'http://foo1.com';
    webUIListenerCallback('preview-deprecated-image', {
      url: 'fake-old-image.jpg',
      author: fakeAuthor,
      website: fakeWebsite,
    });
    flush();

    // Expect the deprecated image is presented in picture pane.
    assertEquals(CrPicture.SelectionTypes.DEPRECATED, crPicturePane.imageType);
    const image = crPicturePane.shadowRoot.querySelector('#image');
    assertTrue(!!image);
    assertFalse(image.hidden);
    const discard = crPicturePane.shadowRoot.querySelector('#discard');
    assertTrue(!!discard);
    assertTrue(discard.hidden);
    const sourceInfo = changePicture.shadowRoot.querySelector('#sourceInfo');
    assertTrue(!!sourceInfo);
    assertFalse(sourceInfo.hidden);
    assertEquals(changePicture.authorInfo_, 'Photo by ' + fakeAuthor);
    assertEquals(changePicture.websiteInfo_, fakeWebsite);
  });

  test('ChangePictureFileImage', async function() {
    assertFalse(!!changePicture.selectedItem_);

    // By default there is no old image and the element is hidden.
    const oldImage = crPictureList.$.oldImage;
    assertTrue(!!oldImage);
    assertTrue(oldImage.hidden);

    webUIListenerCallback('old-image-changed', 'file-image.jpg');
    flush();

    await new Promise(function(resolve) {
      changePicture.async(resolve);
    });
    assertTrue(!!changePicture.selectedItem_);
    // Expect the old image to be selected once an old image is sent via
    // the native interface.
    assertEquals(
        CrPicture.SelectionTypes.OLD, changePicture.selectedItem_.dataset.type);
    assertFalse(oldImage.hidden);
    assertFalse(crPicturePane.cameraActive_);
    const discard = crPicturePane.shadowRoot.querySelector('#discard');
    assertTrue(!!discard);
    assertFalse(discard.hidden);
    // Ensure the file image does not show the source info.
    const sourceInfo = changePicture.shadowRoot.querySelector('#sourceInfo');
    assertTrue(!sourceInfo || sourceInfo.hidden);
  });

  test('ChangePictureSelectFirstDefaultImage', async function() {
    const firstDefaultImage =
        crPictureList.shadowRoot.querySelector('img[data-type="default"]');
    assertTrue(!!firstDefaultImage);

    firstDefaultImage.click();

    let imageUrl = await browserProxy.whenCalled('selectDefaultImage');
    assertEquals('chrome://foo/2.png', imageUrl);

    flush();
    assertEquals(
        CrPicture.SelectionTypes.DEFAULT,
        changePicture.selectedItem_.dataset.type);
    assertEquals(firstDefaultImage, changePicture.selectedItem_);
    assertFalse(crPicturePane.cameraActive_);
    const discard = crPicturePane.shadowRoot.querySelector('#discard');
    assertTrue(!discard || discard.hidden);

    // Now verify that arrow keys actually select the new image.
    browserProxy.resetResolver('selectDefaultImage');
    pressAndReleaseKeyOn(changePicture.selectedItem_, RIGHT_KEY_CODE);
    imageUrl = await browserProxy.whenCalled('selectDefaultImage');
    assertEquals('chrome://foo/3.png', imageUrl);
  });

  test('ChangePictureRestoreImageAfterDiscard', async function() {
    const firstDefaultImage =
        crPictureList.shadowRoot.querySelector('img[data-type="default"]');
    assertTrue(!!firstDefaultImage);

    firstDefaultImage.click();

    await browserProxy.whenCalled('selectDefaultImage');
    flush();
    assertEquals(firstDefaultImage, changePicture.selectedItem_);

    webUIListenerCallback('old-image-changed', 'fake-old-image.jpg');

    flush();
    assertEquals(
        CrPicture.SelectionTypes.OLD, changePicture.selectedItem_.dataset.type);

    const discardButton =
        crPicturePane.shadowRoot.querySelector('#discard cr-icon-button');
    assertTrue(!!discardButton);
    discardButton.click();

    flush();
    const profileImage = crPictureList.$.profileImage;
    assertTrue(!!profileImage);
    assertEquals(profileImage, changePicture.selectedItem_);
  });

  test('ChangePictureImagePendingStateCheck', async function() {
    // oldImagePending_ should be false when no camera photo pending.
    assertFalse(changePicture.oldImagePending_);
    assertEquals(crPictureList.oldImageUrl_, '');
    // Simulate photo taken event.
    crPicturePane.fire('photo-taken', {photoDataUrl: 'camera-image.jpg'});
    flush();
    // oldImagePending_ should be true due to pending camera image.
    assertTrue(changePicture.oldImagePending_);

    webUIListenerCallback('old-image-changed', 'camera-image.jpg');
    flush();
    // oldImagePending_ should be false after the image has been received.
    assertFalse(changePicture.oldImagePending_);
    assertEquals(crPictureList.oldImageUrl_, 'camera-image.jpg');
  });
});

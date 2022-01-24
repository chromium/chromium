// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {CrPicture} from 'chrome://resources/cr_elements/chromeos/cr_picture/cr_picture_types.js';
// #import {down, up, pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js'
// #import {TestBrowserProxy} from '../../test_browser_proxy.js';
// #import {Router, routes, AccountManagerBrowserProxyImpl, ChangePictureBrowserProxyImpl} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

cr.define('settings_people_page_change_picture', function() {
  /** @implements {settings.ChangePictureBrowserProxy} */
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
      cr.webUIListenerCallback(
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
      cr.webUIListenerCallback('default-images-changed', {
        current_default_images: fakeCurrentDefaultImages,
      });

      this.methodCalled('initialize');
    }

    /** @override */
    selectDefaultImage(imageUrl) {
      cr.webUIListenerCallback('selected-image-changed', imageUrl);
      this.methodCalled('selectDefaultImage', imageUrl);
    }

    /** @override */
    selectOldImage() {
      cr.webUIListenerCallback('old-image-changed', {
        url: 'fake-old-image.jpg',
        index: 1,
      });
      this.methodCalled('selectOldImage');
    }

    /** @override */
    selectProfileImage() {
      cr.webUIListenerCallback(
          'profile-image-changed', 'fake-profile-image-url',
          true /* selected */);
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
      settings.ChangePictureBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();
      changePicture = document.createElement('settings-change-picture');
      document.body.appendChild(changePicture);

      crPicturePane = changePicture.$$('cr-picture-pane');
      assertTrue(!!crPicturePane);

      crPictureList = changePicture.$$('cr-picture-list');
      assertTrue(!!crPictureList);

      changePicture.currentRouteChanged(settings.routes.CHANGE_PICTURE);

      await browserProxy.whenCalled('initialize');
      Polymer.dom.flush();
    });

    teardown(function() {
      changePicture.remove();
    });

    test('TraverseCameraIconUsingArrows', function() {
      // Force the camera to be present.
      cr.webUIListenerCallback('camera-presence-changed', true);
      Polymer.dom.flush();
      assertTrue(crPictureList.cameraPresent);

      // Click camera icon.
      const cameraImage = crPictureList.$.cameraImage;
      cameraImage.click();
      Polymer.dom.flush();

      assertTrue(crPictureList.cameraSelected_);
      const crCamera = crPicturePane.$$('#camera');
      assertTrue(!!crCamera);

      // Mock camera's video stream beginning to play.
      crCamera.$.cameraVideo.dispatchEvent(new Event('canplay'));
      Polymer.dom.flush();

      // "Take photo" button should be active.
      let activeElementPath = getActiveElementPath();
      assertTrue(activeElementPath.includes(crPicturePane));
      assertFalse(activeElementPath.includes(crPictureList));

      // Press 'Right' key on active element.
      MockInteractions.pressAndReleaseKeyOn(
          activeElementPath.pop(), RIGHT_KEY_CODE);
      Polymer.dom.flush();

      // A profile picture open should be active.
      activeElementPath = getActiveElementPath();
      assertFalse(crPictureList.cameraSelected_);
      assertFalse(activeElementPath.includes(crPicturePane));
      assertTrue(activeElementPath.includes(crPictureList));

      // Press 'Left' key on active element.
      MockInteractions.pressAndReleaseKeyOn(
          activeElementPath.pop(), LEFT_KEY_CODE);
      Polymer.dom.flush();

      // Mock camera's video stream beginning to play.
      crCamera.$.cameraVideo.dispatchEvent(new Event('canplay'));
      Polymer.dom.flush();

      // "Take photo" button should be active again.
      activeElementPath = getActiveElementPath();
      assertTrue(crPictureList.cameraSelected_);
      assertTrue(activeElementPath.includes(crPicturePane));
      assertFalse(activeElementPath.includes(crPictureList));
    });

    test('ChangePictureSelectCamera', async function() {
      // Force the camera to be absent, even if it's actually present.
      cr.webUIListenerCallback('camera-presence-changed', false);
      Polymer.dom.flush();

      await new Promise(function(resolve) {
        changePicture.async(resolve);
      });
      let camera = crPicturePane.$$('#camera');
      expectFalse(crPicturePane.cameraPresent);
      expectFalse(crPicturePane.cameraActive_);
      expectFalse(!!camera && camera.hidden);

      cr.webUIListenerCallback('camera-presence-changed', true);
      Polymer.dom.flush();
      await new Promise(function(resolve) {
        changePicture.async(resolve);
      });
      camera = crPicturePane.$$('#camera');
      expectTrue(crPicturePane.cameraPresent);
      expectFalse(crPicturePane.cameraActive_);
      expectFalse(!!camera && camera.hidden);

      const cameraImage = crPictureList.$.cameraImage;
      cameraImage.click();
      Polymer.dom.flush();
      await new Promise(function(resolve) {
        changePicture.async(resolve);
      });
      camera = crPicturePane.$$('#camera');
      expectTrue(crPicturePane.cameraActive_);
      assertTrue(!!camera && !camera.hidden);
      expectEquals(
          CrPicture.SelectionTypes.CAMERA,
          changePicture.selectedItem_.dataset.type);
      const discard = crPicturePane.$$('#discard');
      expectTrue(!discard || discard.hidden);

      // Ensure that the camera is deactivated if user navigates away.
      changePicture.currentRouteChanged(settings.routes.BASIC);
      await new Promise(function(resolve) {
        changePicture.async(resolve);
      });
      expectFalse(crPicturePane.cameraActive_);
    });

    test('ChangePictureProfileImage', async function() {
      const profileImage = crPictureList.$.profileImage;
      assertTrue(!!profileImage);

      expectEquals(null, changePicture.selectedItem_);
      profileImage.click();

      await browserProxy.whenCalled('selectProfileImage');
      Polymer.dom.flush();

      expectEquals(
          CrPicture.SelectionTypes.PROFILE,
          changePicture.selectedItem_.dataset.type);
      expectFalse(crPicturePane.cameraActive_);
      const discard = crPicturePane.$$('#discard');
      expectTrue(!discard || discard.hidden);

      // Ensure that the selection is restored after navigating away and
      // then back to the subpage.
      changePicture.currentRouteChanged(settings.routes.BASIC);
      changePicture.currentRouteChanged(settings.routes.CHANGE_PICTURE);
      expectEquals(null, changePicture.selectedItem_);
    });

    test('ChangePictureDeprecatedImage', async function() {
      cr.webUIListenerCallback(
          'preview-deprecated-image', {url: 'fake-old-image.jpg'});
      Polymer.dom.flush();

      // Expect the deprecated image is presented in picture pane.
      expectEquals(
          CrPicture.SelectionTypes.DEPRECATED, crPicturePane.imageType);
      const image = crPicturePane.$$('#image');
      assertTrue(!!image);
      expectFalse(image.hidden);
      const discard = crPicturePane.$$('#discard');
      assertTrue(!!discard);
      expectTrue(discard.hidden);
    });

    test('ChangePictureDeprecatedImageWithSourceInfo', async function() {
      const fakeAuthor = 'FakeAuthor';
      const fakeWebsite = 'http://foo1.com';
      cr.webUIListenerCallback('preview-deprecated-image', {
        url: 'fake-old-image.jpg',
        author: fakeAuthor,
        website: fakeWebsite,
      });
      Polymer.dom.flush();

      // Expect the deprecated image is presented in picture pane.
      expectEquals(
          CrPicture.SelectionTypes.DEPRECATED, crPicturePane.imageType);
      const image = crPicturePane.$$('#image');
      assertTrue(!!image);
      expectFalse(image.hidden);
      const discard = crPicturePane.$$('#discard');
      assertTrue(!!discard);
      expectTrue(discard.hidden);
      const sourceInfo = changePicture.$$('#sourceInfo');
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

      cr.webUIListenerCallback('old-image-changed', 'file-image.jpg');
      Polymer.dom.flush();

      await new Promise(function(resolve) {
        changePicture.async(resolve);
      });
      assertTrue(!!changePicture.selectedItem_);
      // Expect the old image to be selected once an old image is sent via
      // the native interface.
      expectEquals(
          CrPicture.SelectionTypes.OLD,
          changePicture.selectedItem_.dataset.type);
      expectFalse(oldImage.hidden);
      expectFalse(crPicturePane.cameraActive_);
      const discard = crPicturePane.$$('#discard');
      assertTrue(!!discard);
      expectFalse(discard.hidden);
      // Ensure the file image does not show the source info.
      const sourceInfo = changePicture.$$('#sourceInfo');
      assertTrue(!sourceInfo || sourceInfo.hidden);
    });

    test('ChangePictureSelectFirstDefaultImage', async function() {
      const firstDefaultImage = crPictureList.$$('img[data-type="default"]');
      assertTrue(!!firstDefaultImage);

      firstDefaultImage.click();

      let imageUrl = await browserProxy.whenCalled('selectDefaultImage');
      expectEquals('chrome://foo/2.png', imageUrl);

      Polymer.dom.flush();
      expectEquals(
          CrPicture.SelectionTypes.DEFAULT,
          changePicture.selectedItem_.dataset.type);
      expectEquals(firstDefaultImage, changePicture.selectedItem_);
      expectFalse(crPicturePane.cameraActive_);
      const discard = crPicturePane.$$('#discard');
      expectTrue(!discard || discard.hidden);

      // Now verify that arrow keys actually select the new image.
      browserProxy.resetResolver('selectDefaultImage');
      MockInteractions.pressAndReleaseKeyOn(
          changePicture.selectedItem_, RIGHT_KEY_CODE);
      imageUrl = await browserProxy.whenCalled('selectDefaultImage');
      expectEquals('chrome://foo/3.png', imageUrl);
    });

    test('ChangePictureRestoreImageAfterDiscard', async function() {
      const firstDefaultImage = crPictureList.$$('img[data-type="default"]');
      assertTrue(!!firstDefaultImage);

      firstDefaultImage.click();

      await browserProxy.whenCalled('selectDefaultImage');
      Polymer.dom.flush();
      expectEquals(firstDefaultImage, changePicture.selectedItem_);

      cr.webUIListenerCallback('old-image-changed', 'fake-old-image.jpg');

      Polymer.dom.flush();
      expectEquals(
          CrPicture.SelectionTypes.OLD,
          changePicture.selectedItem_.dataset.type);

      const discardButton = crPicturePane.$$('#discard cr-icon-button');
      assertTrue(!!discardButton);
      discardButton.click();

      Polymer.dom.flush();
      const profileImage = crPictureList.$.profileImage;
      assertTrue(!!profileImage);
      expectEquals(profileImage, changePicture.selectedItem_);
    });

    test('ChangePictureImagePendingStateCheck', async function() {
      // oldImagePending_ should be false when no camera photo pending.
      expectFalse(changePicture.oldImagePending_);
      expectEquals(crPictureList.oldImageUrl_, '');
      // Simulate photo taken event.
      crPicturePane.fire('photo-taken', {photoDataUrl: 'camera-image.jpg'});
      Polymer.dom.flush();
      // oldImagePending_ should be true due to pending camera image.
      expectTrue(changePicture.oldImagePending_);

      cr.webUIListenerCallback('old-image-changed', 'camera-image.jpg');
      Polymer.dom.flush();
      // oldImagePending_ should be false after the image has been received.
      expectFalse(changePicture.oldImagePending_);
      expectEquals(crPictureList.oldImageUrl_, 'camera-image.jpg');
    });
  });

  // #cr_define_end
  return {};
});

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

      const fakeDefaultImages = [
        {
          author: 'Author_Old',
          index: 1,
          title: 'Title1',
          url: 'chrome://foo/1.png',
          website: 'http://foo1.com',
        },
        {
          author: 'Author1',
          index: 2,
          title: 'Title2',
          url: 'chrome://foo/2.png',
          website: 'http://foo2.com',
        },
        {
          author: 'Author2',
          index: 3,
          title: 'Title3',
          url: 'chrome://foo/3.png',
          website: 'http://foo3.com',
        },
      ];
      cr.webUIListenerCallback('default-images-changed', {
        images: fakeDefaultImages,
        first: 1,
      });

      this.methodCalled('initialize');
    }

    /** @override */
    selectDefaultImage(imageUrl) {
      cr.webUIListenerCallback('selected-image-changed', imageUrl);
      this.methodCalled('selectDefaultImage', [imageUrl]);
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

    setup(function() {
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

      return browserProxy.whenCalled('initialize').then(function() {
        Polymer.dom.flush();
      });
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

    test('ChangePictureSelectCamera', function() {
      // Force the camera to be absent, even if it's actually present.
      cr.webUIListenerCallback('camera-presence-changed', false);
      Polymer.dom.flush();

      return new Promise(function(resolve) {
               changePicture.async(resolve);
             })
          .then(function() {
            const camera = crPicturePane.$$('#camera');
            expectFalse(crPicturePane.cameraPresent);
            expectFalse(crPicturePane.cameraActive_);
            expectFalse(!!camera && camera.hidden);

            cr.webUIListenerCallback('camera-presence-changed', true);
            Polymer.dom.flush();
            return new Promise(function(resolve) {
              changePicture.async(resolve);
            });
          })
          .then(function() {
            const camera = crPicturePane.$$('#camera');
            expectTrue(crPicturePane.cameraPresent);
            expectFalse(crPicturePane.cameraActive_);
            expectFalse(!!camera && camera.hidden);

            const cameraImage = crPictureList.$.cameraImage;
            cameraImage.click();
            Polymer.dom.flush();
            return new Promise(function(resolve) {
              changePicture.async(resolve);
            });
          })
          .then(function() {
            const camera = crPicturePane.$$('#camera');
            expectTrue(crPicturePane.cameraActive_);
            assertTrue(!!camera && !camera.hidden);
            expectEquals(
                CrPicture.SelectionTypes.CAMERA,
                changePicture.selectedItem_.dataset.type);
            const discard = crPicturePane.$$('#discard');
            expectTrue(!discard || discard.hidden);

            // Ensure that the camera is deactivated if user navigates away.
            changePicture.currentRouteChanged(settings.routes.BASIC);
            return new Promise(function(resolve) {
              changePicture.async(resolve);
            });
          })
          .then(function() {
            expectFalse(crPicturePane.cameraActive_);
          });
    });

    test('ChangePictureProfileImage', function() {
      const profileImage = crPictureList.$.profileImage;
      assertTrue(!!profileImage);

      expectEquals(null, changePicture.selectedItem_);
      profileImage.click();

      return browserProxy.whenCalled('selectProfileImage').then(function() {
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
    });

    test('ChangePictureOldImage', function() {
      assertFalse(!!changePicture.selectedItem_);

      // By default there is no old image and the element is hidden.
      const oldImage = crPictureList.$.oldImage;
      assertTrue(!!oldImage);
      assertTrue(oldImage.hidden);

      cr.webUIListenerCallback('old-image-changed', {
        url: 'fake-old-image.jpg',
        index: 1,
      });
      Polymer.dom.flush();

      return new Promise(function(resolve) {
               changePicture.async(resolve);
             })
          .then(function() {
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
            // Ensure the old image shows the author credit.
            const credit = changePicture.$$('#authorCredit');
            assertTrue(!!credit);
            expectFalse(credit.hidden);
          });
    });

    test('ChangePictureFileImage', function() {
      assertFalse(!!changePicture.selectedItem_);

      // By default there is no old image and the element is hidden.
      const oldImage = crPictureList.$.oldImage;
      assertTrue(!!oldImage);
      assertTrue(oldImage.hidden);

      cr.webUIListenerCallback('old-image-changed', {
        url: 'file-image.jpg',
        index: -1,
      });
      Polymer.dom.flush();

      return new Promise(function(resolve) {
               changePicture.async(resolve);
             })
          .then(function() {
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
            // Ensure the file image does not show the author credit.
            const credit = changePicture.$$('#authorCredit');
            assertTrue(!credit || credit.hidden);
          });
    });

    test('ChangePictureSelectFirstDefaultImage', function() {
      const firstDefaultImage = crPictureList.$$('img[data-type="default"]');
      assertTrue(!!firstDefaultImage);

      firstDefaultImage.click();

      return browserProxy.whenCalled('selectDefaultImage')
          .then(function(args) {
            expectEquals('chrome://foo/2.png', args[0]);

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
            return browserProxy.whenCalled('selectDefaultImage');
          })
          .then(function(args) {
            expectEquals('chrome://foo/3.png', args[0]);
          });
    });

    test('ChangePictureRestoreImageAfterDiscard', function() {
      const firstDefaultImage = crPictureList.$$('img[data-type="default"]');
      assertTrue(!!firstDefaultImage);

      firstDefaultImage.click();

      return browserProxy.whenCalled('selectDefaultImage').then(function() {
        Polymer.dom.flush();
        expectEquals(firstDefaultImage, changePicture.selectedItem_);

        cr.webUIListenerCallback('old-image-changed', {
          url: 'fake-old-image.jpg',
          index: 1,
        });

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
    });
  });
});

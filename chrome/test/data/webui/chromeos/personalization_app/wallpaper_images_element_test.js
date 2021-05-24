// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {untrustedOrigin} from 'chrome://personalization/common/constants.js';
import {selectImage} from 'chrome://personalization/common/iframe_api.js';
import {ActionName} from 'chrome://personalization/trusted/personalization_actions.js';
import {promisifySendImagesForTesting, WallpaperImages} from 'chrome://personalization/trusted/wallpaper_images_element.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotReached, assertTrue} from '../../chai_assert.js';
import {flushTasks, waitAfterNextRender} from '../../test_util.m.js';
import {assertWindowObjectsEqual, baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestWallpaperProvider} from './test_mojo_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

const fetchImagesForCollection = 'fetchImagesForCollection';

export function WallpaperImagesTest() {
  /** @type {?HTMLElement} */
  let wallpaperImagesElement = null;

  /** @type {?TestWallpaperProvider} */
  let wallpaperProvider = null;

  /** @type {?TestPersonalizationStore} */
  let personalizationStore = null;

  /**
   * Promisify the message event and delegate to the original onImageSelected_
   * handler. Returns a promise that is resolved the next time onImageSelected_
   * completes. Leaves the message handler unset at the end.
   * @return {!Promise<null>}
   */
  function promisifyMessageEventOnce() {
    const original = wallpaperImagesElement.onImageSelected_;
    window.removeEventListener('message', original);
    return new Promise((resolve, reject) => {
      function patched(event) {
        // Rewrite to untrusted to pass validation of message origin.
        const newEvent = {data: event.data, origin: untrustedOrigin};
        return original.call(wallpaperImagesElement, newEvent)
            .then(resolve, reject);
      }
      window.addEventListener('message', patched, {once: true});
    });
  }

  setup(() => {
    const mocks = baseSetup();
    wallpaperProvider = mocks.wallpaperProvider;
    personalizationStore = mocks.personalizationStore;
  });

  teardown(async () => {
    await teardownElement(wallpaperImagesElement);
    wallpaperImagesElement = null;
    await flushTasks();
  });

  test('fetches wallpaper images on collection-id change', async () => {
    wallpaperImagesElement =
        initElement(WallpaperImages.is, {active: true, collectionId: 'id_0'});

    let collectionId =
        await wallpaperProvider.whenCalled(fetchImagesForCollection);
    assertEquals('id_0', collectionId);
    assertEquals(1, wallpaperProvider.getCallCount(fetchImagesForCollection));
    await waitAfterNextRender(wallpaperImagesElement);

    wallpaperProvider.resetResolver(fetchImagesForCollection);

    wallpaperImagesElement.collectionId = 'id_1';
    collectionId = await wallpaperProvider.whenCalled(fetchImagesForCollection);
    assertEquals('id_1', collectionId);
    assertEquals(1, wallpaperProvider.getCallCount(fetchImagesForCollection));
  });

  test('displays images for current collection id', async () => {
    let sendImagesPromise = promisifySendImagesForTesting();
    wallpaperImagesElement =
        initElement(WallpaperImages.is, {active: true, collectionId: 'id_0'});

    const iframe =
        wallpaperImagesElement.shadowRoot.getElementById('images-iframe');

    // Wait for the call to fetch images.
    let requestedId =
        await wallpaperProvider.whenCalled(fetchImagesForCollection);
    assertEquals('id_0', requestedId);

    // Wait for iframe to receive data.
    let [targetWindow, data] = await sendImagesPromise;
    assertEquals(iframe.contentWindow, targetWindow);
    assertDeepEquals(wallpaperProvider.images, data);
    // Wait for a render to happen.
    await waitAfterNextRender(wallpaperImagesElement);
    assertFalse(iframe.hidden);

    wallpaperProvider.resetResolver(fetchImagesForCollection);
    sendImagesPromise = promisifySendImagesForTesting();
    wallpaperProvider.setImages([
      {assetId: BigInt(10), url: {url: 'https://id_1-0/'}},
      {assetId: BigInt(20), url: {url: 'https://id_1-1/'}},
    ]);
    wallpaperImagesElement.collectionId = 'id_1';

    // Wait for another call to fetch images.
    requestedId = await wallpaperProvider.whenCalled(fetchImagesForCollection);
    assertEquals('id_1', requestedId);
    // Wait for iframe to receive new data.
    [targetWindow, data] = await sendImagesPromise;

    await waitAfterNextRender(wallpaperImagesElement);

    assertFalse(iframe.hidden);

    assertWindowObjectsEqual(iframe.contentWindow, targetWindow);
    assertDeepEquals(wallpaperProvider.images, data);
  });

  test('displays error on loading failure', async () => {
    const sendImagesPromise = promisifySendImagesForTesting();
    wallpaperProvider.setImagesToFail();

    wallpaperImagesElement =
        initElement(WallpaperImages.is, {active: true, collectionId: 'id_0'});

    const spinner =
        wallpaperImagesElement.shadowRoot.querySelector('paper-spinner-lite');
    assertTrue(spinner.active);

    const error = wallpaperImagesElement.shadowRoot.querySelector('#error');
    assertTrue(error.hidden);

    await wallpaperProvider.whenCalled(fetchImagesForCollection);
    const [_, images] = await sendImagesPromise;
    await waitAfterNextRender(wallpaperImagesElement);

    assertDeepEquals([], images, 'Empty image array should have been sent');

    // The iframe should be hidden.
    const iframe = wallpaperImagesElement.shadowRoot.querySelector('iframe');
    assertTrue(iframe.hidden);

    assertFalse(spinner.active);
    assertFalse(error.hidden);
  });

  test('throws error when invalid SelectImageEvent is received', async () => {
    const sendImagesPromise = promisifySendImagesForTesting();
    wallpaperImagesElement =
        initElement(WallpaperImages.is, {active: true, collectionId: 'id_0'});

    const selectImagePromise = promisifyMessageEventOnce().then(
        () => {
          assertNotReached();
          return false;
        },
        (e) => {
          assertEquals(
              'Assertion failed: No valid selection found in choices',
              e.message);
          return true;
        });
    await wallpaperProvider.whenCalled(fetchImagesForCollection);
    await sendImagesPromise;
    await waitAfterNextRender(wallpaperImagesElement);

    // This image assetId should fail validation.
    selectImage(window, /*assetId=*/ BigInt(-10));

    // Wait for the message handler |patched| to run.
    const result = await selectImagePromise;
    assertTrue(result);
  });

  test(
      'calls SelectWallpaper when valid SelectImageEvent is received',
      async () => {
        wallpaperImagesElement = initElement(WallpaperImages.is, {
          active: true,
          collectionId: 'id_0',
          onWallpaperSet() {},
        });

        const selectImagePromise = promisifyMessageEventOnce().then(() => {
          return wallpaperProvider.whenCalled('selectWallpaper');
        });

        await wallpaperProvider.whenCalled(fetchImagesForCollection);
        await waitAfterNextRender(wallpaperImagesElement);

        selectImage(
            window, /*assetId=*/
            wallpaperProvider.images[0].assetId);

        const assetId = await selectImagePromise;
        assertEquals(wallpaperProvider.images[0].assetId, assetId);
      });

  test('updates store after successful image selection', async () => {
    wallpaperImagesElement =
        initElement(WallpaperImages.is, {active: true, collectionId: 'id_0'});

    promisifyMessageEventOnce();

    await wallpaperProvider.whenCalled(fetchImagesForCollection);
    await waitAfterNextRender(wallpaperImagesElement);

    personalizationStore.expectAction(ActionName.SET_CURRENT_IMAGE);

    selectImage(
        window, /*assetId=*/
        wallpaperProvider.images[0].assetId);

    const action =
        await personalizationStore.waitForAction(ActionName.SET_CURRENT_IMAGE);
    assertEquals(wallpaperProvider.images[0], action.image);
  });
}

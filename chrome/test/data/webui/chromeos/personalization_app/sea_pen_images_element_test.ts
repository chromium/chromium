// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {SeaPenImageLoadingElement, SeaPenImagesElement, SeaPenZeroStateSvgElement, setSelectedRecentSeaPenImageAction, SparklePlaceholderElement, WallpaperGridItemElement} from 'chrome://personalization/js/personalization_app.js';
import {CrIconButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {MantaStatusCode, SeaPenThumbnail} from 'chrome://resources/ash/common/sea_pen/sea_pen.mojom-webui.js';
import {PaperSpinnerLiteElement} from 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestSeaPenProvider} from './test_sea_pen_interface_provider.js';

suite('SeaPenImagesElementTest', function() {
  let personalizationStore: TestPersonalizationStore;
  let seaPenProvider: TestSeaPenProvider;
  let seaPenImagesElement: SeaPenImagesElement|null;

  function getWallpaperGridItems(): WallpaperGridItemElement[] {
    return Array.from(seaPenImagesElement!.shadowRoot!
                          .querySelectorAll<WallpaperGridItemElement>(
                              `div:not([hidden]).thumbnail-item-container ` +
                              `wallpaper-grid-item:not([hidden])`));
  }

  function getThumbnailLoadingElements(): SeaPenImageLoadingElement[] {
    return Array.from(seaPenImagesElement!.shadowRoot!.querySelectorAll<
                      SeaPenImageLoadingElement>(
        `div:not([hidden]).thumbnail-item-container sea-pen-image-loading`));
  }

  setup(() => {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    seaPenProvider = mocks.seaPenProvider;
  });

  teardown(async () => {
    await teardownElement(seaPenImagesElement);
    seaPenImagesElement = null;
  });

  test('displays zero state SVG', async () => {
    // Initialize |seaPenImagesElement|.
    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    const loadingThumbnailPlaceholder =
        seaPenImagesElement.shadowRoot!.querySelector('.placeholder');
    assertFalse(
        !!loadingThumbnailPlaceholder,
        'thumbnails should not be in loading state');

    assertTrue(
        !!seaPenImagesElement.shadowRoot!.querySelector(
            SeaPenZeroStateSvgElement.is),
        'sea-pen-zero-state-svg is shown initially');
  });

  test('displays loading thumbnail placeholders', async () => {
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = true;
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.images;

    // Initialize |seaPenImagesElement|.
    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    const loadingThumbnailPlaceholders =
        seaPenImagesElement.shadowRoot!
            .querySelectorAll<SparklePlaceholderElement>(
                'div:not([hidden]) sparkle-placeholder');
    assertEquals(
        8, loadingThumbnailPlaceholders!.length,
        'should be 8 loading placeholders available.');
    assertTrue(Array.from(loadingThumbnailPlaceholders)
                   .every(placeholder => !!placeholder.active));
  });

  test('thumbnail placeholders not active when hidden', async () => {
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = false;
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.images;

    // Initialize |seaPenImagesElement|.
    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    const loadingThumbnailPlaceholders =
        seaPenImagesElement.shadowRoot!
            .querySelectorAll<SparklePlaceholderElement>(
                'div:not([hidden]) sparkle-placeholder');
    assertTrue(Array.from(loadingThumbnailPlaceholders)
                   .every(placeholder => !placeholder.active));
  });

  test('displays image thumbnails', async () => {
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = false;
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.images;

    // Initialize |seaPenImagesElement|.
    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    const thumbnails = seaPenImagesElement.shadowRoot!.querySelectorAll(
        'div:not([hidden]).thumbnail-item-container');
    assertEquals(4, thumbnails!.length, 'should be 4 images available.');
  });

  test('manages loading and selected when thumbnail clicked', async () => {
    personalizationStore.setReducersEnabled(true);
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = false;
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.images;
    // Index 1 is currently set as wallpaper.
    personalizationStore.data.wallpaper.seaPen.currentSelected =
        seaPenProvider.images[1]!.id;

    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    let thumbnails = getWallpaperGridItems();
    assertEquals(4, thumbnails!.length, 'should be 4 images available');
    let imageThumbnailGrid =
        seaPenImagesElement!.shadowRoot!.querySelector('#grid');
    assertTrue(
        isVisible(imageThumbnailGrid!), 'thumbnail grid should be visible');
    assertDeepEquals(
        [false, true, false, false],
        thumbnails.map(thumbnail => thumbnail.selected),
        'index 1 thumbnail shows as selected');

    let thumbnailSelectedLoadingElement: SeaPenImageLoadingElement[] =
        getThumbnailLoadingElements();
    assertEquals(
        0, thumbnailSelectedLoadingElement!.length,
        'should be 0 loading elements');

    // Simulate the request starting with a user click on a thumbnail.
    const selectSeaPenThumbnailResolver =
        new PromiseResolver<{success: boolean}>();
    seaPenProvider.selectSeaPenThumbnailResponse =
        selectSeaPenThumbnailResolver.promise;
    thumbnails[0]!.click();
    await waitAfterNextRender(seaPenImagesElement);

    thumbnails = getWallpaperGridItems();
    assertEquals(4, thumbnails!.length, 'still 4 images available after click');
    imageThumbnailGrid =
        seaPenImagesElement!.shadowRoot!.querySelector('#grid');
    assertTrue(
        isVisible(imageThumbnailGrid!), 'thumbnail grid should be visible');
    assertDeepEquals(
        [true, false, false, false],
        thumbnails.map(thumbnail => thumbnail.selected),
        'index 0 thumbnail shows as selected after click');

    thumbnailSelectedLoadingElement = getThumbnailLoadingElements();
    assertEquals(
        1, thumbnailSelectedLoadingElement!.length,
        'should be 1 loading element');
    const spinner: PaperSpinnerLiteElement|null =
        thumbnailSelectedLoadingElement[0]!.shadowRoot!.querySelector(
            'paper-spinner-lite:not([hidden])');
    assertTrue(!!spinner, 'there should be a spinner in the loading element');
    const loadingText =
        thumbnailSelectedLoadingElement[0]!.shadowRoot!.querySelector(
            'p:not([hidden])');
    assertEquals(
        seaPenImagesElement.i18n('seaPenCreatingHighResImage'),
        loadingText!.textContent, 'the loading text should be correct');
    assertEquals(
        (personalizationStore.data.wallpaper.seaPen.pendingSelected as
         SeaPenThumbnail)
            .image,
        thumbnailSelectedLoadingElement[0]!.parentElement
            ?.querySelector('wallpaper-grid-item')
            ?.src,
        'sibling wallpaper-grid-item has expected src');
    assertEquals(
        true,
        thumbnailSelectedLoadingElement[0]!.parentElement
            ?.querySelector('wallpaper-grid-item')
            ?.selected,
        'sibling wallpaper-grid-item is selected');

    // Simulate the request resolving.
    selectSeaPenThumbnailResolver.resolve({success: true});
    await waitAfterNextRender(seaPenImagesElement);
    // Simulate receiving a confirmation that the sea pen image was selected.
    personalizationStore.dispatch(
        setSelectedRecentSeaPenImageAction(seaPenProvider.images[0]!.id));
    await waitAfterNextRender(seaPenImagesElement);

    thumbnails = getWallpaperGridItems();
    assertEquals(
        4, thumbnails!.length, 'still 4 images available after resolve');
    imageThumbnailGrid =
        seaPenImagesElement!.shadowRoot!.querySelector('#grid');
    assertTrue(
        isVisible(imageThumbnailGrid!), 'thumbnail grid should be visible');
    assertDeepEquals(
        [true, false, false, false],
        thumbnails.map(thumbnail => thumbnail.selected),
        'index 0 thumbnail still selected after resolve');
    thumbnailSelectedLoadingElement = getThumbnailLoadingElements();
    assertEquals(
        0, thumbnailSelectedLoadingElement!.length, 'no more loading element');
  });

  test('display feedback buttons', async () => {
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = false;
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.images;

    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    const feedbackButtons: CrIconButtonElement[] = Array.from(
        seaPenImagesElement.shadowRoot!.querySelectorAll<CrIconButtonElement>(
            `div:not([hidden]).thumbnail-item-container sea-pen-feedback`));
    assertTrue(feedbackButtons.length > 0);
  });

  test('display no network error state', async () => {
    personalizationStore.data.wallpaper.seaPen.thumbnailResponseStatusCode =
        MantaStatusCode.kNoInternetConnection;

    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    const errorMessage = seaPenImagesElement.shadowRoot!.querySelector(
                             '.error-message') as HTMLElement;
    assertTrue(!!errorMessage);
    assertEquals(
        seaPenImagesElement.i18n('seaPenErrorNoInternet'),
        errorMessage!.innerText);
    const imageHeading = seaPenImagesElement.shadowRoot!.querySelector(
                             '.wallpaper-collections-heading') as HTMLElement;
    assertFalse(
        !!imageHeading,
        'image heading should not be displayed when an error is present');
  });

  test('display resource exhausted error state', async () => {
    personalizationStore.data.wallpaper.seaPen.thumbnailResponseStatusCode =
        MantaStatusCode.kResourceExhausted;

    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    const errorMessage = seaPenImagesElement.shadowRoot!.querySelector(
                             '.error-message') as HTMLElement;
    assertTrue(!!errorMessage, 'an error message should be displayed');
    assertEquals(
        seaPenImagesElement.i18n('seaPenErrorResourceExhausted'),
        errorMessage!.innerText);
    const imageHeading = seaPenImagesElement.shadowRoot!.querySelector(
                             '.wallpaper-collections-heading') as HTMLElement;
    assertFalse(
        !!imageHeading,
        'image heading should not be displayed when an error is present');
  });

  test('display user quota exceeded error state', async () => {
    personalizationStore.data.wallpaper.seaPen.thumbnailResponseStatusCode =
        MantaStatusCode.kPerUserQuotaExceeded;

    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    const errorMessage = seaPenImagesElement.shadowRoot!.querySelector(
                             '.error-message') as HTMLElement;
    assertTrue(!!errorMessage, 'an error message should be displayed');
    assertEquals(
        seaPenImagesElement.i18n('seaPenErrorResourceExhausted'),
        errorMessage!.innerText);
    const imageHeading = seaPenImagesElement.shadowRoot!.querySelector(
                             '.wallpaper-collections-heading') as HTMLElement;
    assertFalse(
        !!imageHeading,
        'image heading should not be displayed when an error is present');
  });

  test('display generic error state', async () => {
    personalizationStore.data.wallpaper.seaPen.thumbnailResponseStatusCode =
        MantaStatusCode.kGenericError;

    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    const errorMessage = seaPenImagesElement.shadowRoot!.querySelector(
                             '.error-message') as HTMLElement;
    assertTrue(!!errorMessage, 'an error message should be displayed');
    assertEquals(
        seaPenImagesElement.i18n('seaPenErrorGeneric'),
        errorMessage!.innerText);
    const imageHeading = seaPenImagesElement.shadowRoot!.querySelector(
                             '.wallpaper-collections-heading') as HTMLElement;
    assertFalse(
        !!imageHeading,
        'image heading should not be displayed when an error is present');
  });

  test('hide error state on success', async () => {
    personalizationStore.data.wallpaper.seaPen.thumbnailResponseStatusCode =
        MantaStatusCode.kOk;

    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    const errorMessage =
        seaPenImagesElement.shadowRoot!.querySelector('.error-message');
    assertFalse(!!errorMessage, 'error messages should be hidden on success');
  });

  test('hide error state while loading', async () => {
    personalizationStore.data.wallpaper.seaPen.thumbnailResponseStatusCode =
        MantaStatusCode.kGenericError;
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = true;

    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    const errorMessage =
        seaPenImagesElement.shadowRoot!.querySelector('.error-message');
    assertFalse(
        !!errorMessage, 'error messages should be hidden while loading');
  });
});

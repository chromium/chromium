// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {SeaPenErrorElement, SeaPenHistoryPromptSelectedEvent, SeaPenImageLoadingElement, SeaPenImagesElement, SeaPenRouterElement, SeaPenZeroStateSvgElement, setSeaPenThumbnailsAction, setSelectedRecentSeaPenImageAction, setTransitionsEnabled, SparklePlaceholderElement, WallpaperGridItemElement} from 'chrome://personalization/js/personalization_app.js';
import {CrIconButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {MantaStatusCode, SeaPenThumbnail} from 'chrome://resources/ash/common/sea_pen/sea_pen.mojom-webui.js';
import {SeaPenTemplateId} from 'chrome://resources/ash/common/sea_pen/sea_pen_generated.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PaperSpinnerLiteElement} from 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

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
    loadTimeData.overrideValues({isSeaPenEnabled: true});
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    seaPenProvider = mocks.seaPenProvider;
    // Disables animation by default.
    setTransitionsEnabled(false);
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
    assertTrue(
        !!seaPenImagesElement.shadowRoot!.querySelector('.zero-state-message'),
        'zero state message is shown');
  });

  test('displays 8 loading thumbnail placeholders for template', async () => {
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = true;
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.thumbnails;

    // Initialize |seaPenImagesElement|.
    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    const loadingThumbnailPlaceholders =
        seaPenImagesElement.shadowRoot!
            .querySelectorAll<SparklePlaceholderElement>(
                'div:not([hidden]) .loading-placeholder > sparkle-placeholder');
    assertEquals(
        8, loadingThumbnailPlaceholders!.length,
        'should be 8 loading placeholders available.');
    assertTrue(Array.from(loadingThumbnailPlaceholders)
                   .every(placeholder => !!placeholder.active));
  });

  test('displays 4 loading thumbnail placeholders for freeform', async () => {
    loadTimeData.overrideValues({isSeaPenTextInputEnabled: true});
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = true;
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.thumbnails;

    seaPenImagesElement =
        initElement(SeaPenImagesElement, {templateId: 'Query'});
    await waitAfterNextRender(seaPenImagesElement);

    const loadingThumbnailPlaceholders =
        seaPenImagesElement.shadowRoot!
            .querySelectorAll<SparklePlaceholderElement>(
                'div:not([hidden]) .loading-placeholder > sparkle-placeholder');
    assertEquals(
        4, loadingThumbnailPlaceholders!.length,
        'should be 4 loading placeholders available.');
    assertTrue(Array.from(loadingThumbnailPlaceholders)
                   .every(placeholder => !!placeholder.active));
  });

  test('thumbnail placeholders not active when hidden', async () => {
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = false;
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.thumbnails;

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
        seaPenProvider.thumbnails;

    // Initialize |seaPenImagesElement|.
    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    const thumbnails = seaPenImagesElement.shadowRoot!.querySelectorAll(
        'div:not([hidden]).thumbnail-item-container');
    assertEquals(4, thumbnails!.length, 'should be 4 images available.');
  });

  test('displays freeform history', async () => {
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = false;
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.thumbnails;
    personalizationStore.data.wallpaper.seaPen.currentSeaPenQuery = {
      textQuery: 'test freeform query',
    };
    personalizationStore.data.wallpaper.seaPen.textQueryHistory = [
      {
        query: 'test freeform query',
        thumbnails: personalizationStore.data.wallpaper.seaPen.thumbnails =
            seaPenProvider.thumbnails,
      },
      {
        query: 'test freeform query 1',
        thumbnails: personalizationStore.data.wallpaper.seaPen.thumbnails =
            seaPenProvider.thumbnails,
      },
    ];

    // Initialize |seaPenImagesElement|.
    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    const lastQuery =
        seaPenImagesElement.shadowRoot!.getElementById('queryHistoryHeading0');
    assertTrue(!!lastQuery, 'last query is available');
    assertEquals(
        'test freeform query', lastQuery.textContent?.trim(),
        'unexpected query');
    const thumbnails = seaPenImagesElement.shadowRoot!.querySelectorAll(
        'div:not([hidden]).thumbnail-item-container.history-item');
    assertEquals(8, thumbnails!.length, 'should be 8 images available.');
  });

  test(
      'sends history prompt selected event when history prompt is clicked',
      async () => {
        personalizationStore.data.wallpaper.seaPen.loading.thumbnails = false;
        personalizationStore.data.wallpaper.seaPen.thumbnails =
            seaPenProvider.thumbnails;
        personalizationStore.data.wallpaper.seaPen.currentSeaPenQuery = {
          textQuery: 'test freeform query',
        };
        personalizationStore.data.wallpaper.seaPen.textQueryHistory = [
          {
            query: 'test freeform query',
            thumbnails: personalizationStore.data.wallpaper.seaPen.thumbnails =
                seaPenProvider.thumbnails,
          },
          {
            query: 'test freeform query 1',
            thumbnails: personalizationStore.data.wallpaper.seaPen.thumbnails =
                seaPenProvider.thumbnails,
          },
        ];

        // Initialize |seaPenImagesElement|.
        seaPenImagesElement = initElement(SeaPenImagesElement);
        await waitAfterNextRender(seaPenImagesElement);

        const lastQuery = seaPenImagesElement.shadowRoot!.getElementById(
            'queryHistoryHeading0');
        assertTrue(!!lastQuery, 'last query is available');

        const historyPromptSelectedEvent = eventToPromise(
            SeaPenHistoryPromptSelectedEvent.EVENT_NAME, seaPenImagesElement);

        lastQuery.click();

        await historyPromptSelectedEvent;
      });

  test('manages loading and selected when thumbnail clicked', async () => {
    personalizationStore.setReducersEnabled(true);
    seaPenImagesElement = initElement(SeaPenImagesElement, {templateId: 10});
    await waitAfterNextRender(seaPenImagesElement);

    // Simulate a query was run and we got the thumbnails, one of the sea pen
    // thumbnails was selected.
    personalizationStore.dispatch(setSeaPenThumbnailsAction(
        seaPenProvider.seaPenQuery, seaPenProvider.thumbnails));
    personalizationStore.dispatch(
        setSelectedRecentSeaPenImageAction(seaPenProvider.thumbnails[1]!.id));
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
        setSelectedRecentSeaPenImageAction(seaPenProvider.thumbnails[0]!.id));
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
    loadTimeData.overrideValues({isManagedSeaPenFeedbackEnabled: true});
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = false;
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.thumbnails;

    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    const feedbackButtons: CrIconButtonElement[] = Array.from(
        seaPenImagesElement.shadowRoot!.querySelectorAll<CrIconButtonElement>(
            `div:not([hidden]).thumbnail-item-container sea-pen-feedback`));
    assertTrue(feedbackButtons.length > 0);
  });

  test('hide feedback buttons if managed feedback disabled', async () => {
    loadTimeData.overrideValues({isManagedSeaPenFeedbackEnabled: false});
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = false;
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.thumbnails;

    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    assertFalse(
        !!seaPenImagesElement.shadowRoot!.querySelector<CrIconButtonElement>(
            `div:not([hidden]).thumbnail-item-container sea-pen-feedback`));
  });

  test('hide error state on success', async () => {
    personalizationStore.data.wallpaper.seaPen.thumbnailResponseStatusCode =
        MantaStatusCode.kOk;

    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    const errorState =
        seaPenImagesElement.shadowRoot!.querySelector(SeaPenErrorElement.is);
    assertFalse(!!errorState, 'error state should be hidden on success');
  });

  test('hide error state while loading', async () => {
    personalizationStore.data.wallpaper.seaPen.thumbnailResponseStatusCode =
        MantaStatusCode.kGenericError;
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = true;

    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    const errorState =
        seaPenImagesElement.shadowRoot!.querySelector(SeaPenErrorElement.is);
    assertFalse(!!errorState, 'error state should be hidden while loading');
  });

  test('switching templates while loading resets loading state', async () => {
    personalizationStore.setReducersEnabled(true);
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = true;
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.thumbnails;

    // Initialize |seaPenImagesElement|.
    seaPenImagesElement = initElement(SeaPenImagesElement);
    initElement(SeaPenRouterElement, {basePath: '/base'});
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

    SeaPenRouterElement.instance().selectSeaPenTemplate(
        SeaPenTemplateId.kGlowscapes);
    await waitAfterNextRender(seaPenImagesElement);

    assertFalse(
        personalizationStore.data.wallpaper.seaPen.loading.thumbnails,
        'thumbnails should no longer be loading');
    const loadingThumbnailPlaceholder =
        seaPenImagesElement.shadowRoot!.querySelector('.placeholder');
    assertFalse(
        !!loadingThumbnailPlaceholder,
        'thumbnails should not be in loading state');

    assertTrue(
        !!seaPenImagesElement.shadowRoot!.querySelector(
            SeaPenZeroStateSvgElement.is),
        'sea-pen-zero-state-svg is shown');
    assertTrue(
        !!seaPenImagesElement.shadowRoot!.querySelector('.zero-state-message'),
        'zero state message is shown');
  });

  test('show images heading', async () => {
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = false;
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.thumbnails;

    // Initialize |seaPenImagesElement|.
    seaPenImagesElement = initElement(SeaPenImagesElement);
    await waitAfterNextRender(seaPenImagesElement);

    assertTrue(
        !!seaPenImagesElement.shadowRoot!.querySelector('#seaPenImagesHeading'),
        'seaPenImagesHeading is shown');
  });

  test('show images heading for template query', async () => {
    loadTimeData.overrideValues({isSeaPenTextInputEnabled: true});
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = false;
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.thumbnails;

    // Initialize |seaPenImagesElement|.
    seaPenImagesElement = initElement(SeaPenImagesElement, {templateId: 3});
    await waitAfterNextRender(seaPenImagesElement);

    assertTrue(
        !!seaPenImagesElement.shadowRoot!.querySelector('#seaPenImagesHeading'),
        'seaPenImagesHeading is shown');
  });

  test('hide images heading for freeform query', async () => {
    loadTimeData.overrideValues({isSeaPenTextInputEnabled: true});
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = false;
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.thumbnails;

    // Initialize |seaPenImagesElement|.
    seaPenImagesElement =
        initElement(SeaPenImagesElement, {templateId: 'Query'});
    await waitAfterNextRender(seaPenImagesElement);

    assertFalse(
        !!seaPenImagesElement.shadowRoot!.querySelector('#seaPenImagesHeading'),
        'seaPenImagesHeading is hidden');
  });
});

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {emptyState, Paths, PersonalizationRouterElement, QueryParams, SeaPenActionName, SeaPenState, WallpaperSubpageTopElement} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

suite('WallpaperSubpageTopElementTest', function() {
  let personalizationStore: TestPersonalizationStore;
  let wallpaperSubpageTopElement: WallpaperSubpageTopElement|null;

  setup(() => {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
  });

  teardown(async () => {
    await teardownElement(wallpaperSubpageTopElement);
    wallpaperSubpageTopElement = null;
  });

  test('hides SeaPen for ineligible users', async () => {
    loadTimeData.overrideValues({isSeaPenEnabled: false});
    wallpaperSubpageTopElement = initElement(
        WallpaperSubpageTopElement, {path: Paths.SEA_PEN_COLLECTION});
    await waitAfterNextRender(wallpaperSubpageTopElement);

    // wallpaper selected page is displayed.
    const wallpaperSelected =
        wallpaperSubpageTopElement!.shadowRoot!.querySelector(
            'wallpaper-selected');
    assertTrue(
        !!wallpaperSelected, 'wallpaper selected element should be displayed');

    // SeaPen input should not be displayed.
    const seaPenInputQueryElement =
        wallpaperSubpageTopElement.shadowRoot!.querySelector(
            'sea-pen-input-query');
    assertFalse(!!seaPenInputQueryElement, 'input query should not display.');

    // SeaPen template should not be displayed.
    const seaPenTemplateQueryElement =
        wallpaperSubpageTopElement.shadowRoot!.querySelector(
            'sea-pen-template-query');
    assertFalse(
        !!seaPenTemplateQueryElement, 'template query should not display.');
  });


  test('hides SeaPen input for ineligible users', async () => {
    loadTimeData.overrideValues(
        {isSeaPenEnabled: true, isSeaPenTextInputEnabled: false});
    wallpaperSubpageTopElement = initElement(
        WallpaperSubpageTopElement, {path: Paths.SEA_PEN_COLLECTION});
    await waitAfterNextRender(wallpaperSubpageTopElement);

    // wallpaper selected page is displayed.
    const wallpaperSelected =
        wallpaperSubpageTopElement!.shadowRoot!.querySelector(
            'wallpaper-selected');
    assertTrue(
        !!wallpaperSelected, 'wallpaper selected element should be displayed');

    // SeaPen input should not be displayed.
    const seaPenInputQueryElement =
        wallpaperSubpageTopElement.shadowRoot!.querySelector(
            'sea-pen-input-query');
    assertFalse(!!seaPenInputQueryElement, 'input query should not display.');

    // SeaPen template should not be displayed.
    const seaPenTemplateQueryElement =
        wallpaperSubpageTopElement.shadowRoot!.querySelector(
            'sea-pen-template-query');
    assertFalse(
        !!seaPenTemplateQueryElement, 'template query should not display.');
  });

  test('hides SeaPen on other paths', async () => {
    loadTimeData.overrideValues(
        {isSeaPenEnabled: true, isSeaPenTextInputEnabled: true});
    wallpaperSubpageTopElement = initElement(
        WallpaperSubpageTopElement, {path: Paths.GOOGLE_PHOTOS_COLLECTION});
    await waitAfterNextRender(wallpaperSubpageTopElement);

    // wallpaper selected page is displayed.
    const wallpaperSelected =
        wallpaperSubpageTopElement!.shadowRoot!.querySelector(
            'wallpaper-selected');
    assertTrue(
        !!wallpaperSelected, 'wallpaper selected element should be displayed');

    // SeaPen input is not displayed.
    const seaPenInputQueryElement =
        wallpaperSubpageTopElement.shadowRoot!.querySelector(
            'sea-pen-input-query');
    assertFalse(!!seaPenInputQueryElement, 'input query should not display.');

    // SeaPen template should not be displayed.
    const seaPenTemplateQueryElement =
        wallpaperSubpageTopElement.shadowRoot!.querySelector(
            'sea-pen-template-query');
    assertFalse(
        !!seaPenTemplateQueryElement, 'template query should not display.');
  });

  test('shows SeaPen input', async () => {
    loadTimeData.overrideValues(
        {isSeaPenEnabled: true, isSeaPenTextInputEnabled: true});
    wallpaperSubpageTopElement = initElement(
        WallpaperSubpageTopElement, {path: Paths.SEA_PEN_COLLECTION});
    await waitAfterNextRender(wallpaperSubpageTopElement);

    // wallpaper selected page isn't displayed.
    const wallpaperSelected =
        wallpaperSubpageTopElement!.shadowRoot!.querySelector(
            'wallpaper-selected');
    assertFalse(
        !!wallpaperSelected,
        'wallpaper selected element should not be displayed.');

    // SeaPen input is displayed.
    const seaPenInputQueryElement =
        wallpaperSubpageTopElement.shadowRoot!.querySelector(
            'sea-pen-input-query');
    assertTrue(!!seaPenInputQueryElement, 'input query should be displayed.');
    // Template query should be hidden.
    const templateQuery = wallpaperSubpageTopElement.shadowRoot!.querySelector(
        'sea-pen-template-query');
    assertFalse(!!templateQuery, 'template query should not be displayed.');
    // Verify that the text input area and search button are displayed.
    const inputQuery =
        seaPenInputQueryElement!.shadowRoot!.querySelector('cr-input');
    assertTrue(!!inputQuery, 'input query should display.');
    const searchButton = seaPenInputQueryElement!.shadowRoot!.querySelector(
                             '#searchButton') as HTMLElement;
    assertTrue(!!searchButton, 'search button should display.');
  });

  test('shows input element on sea pen results page', async () => {
    loadTimeData.overrideValues(
        {isSeaPenEnabled: true, isSeaPenTextInputEnabled: true});
    wallpaperSubpageTopElement = initElement(WallpaperSubpageTopElement, {
      path: Paths.SEA_PEN_RESULTS,
      'templateId': 'Query',
    });
    await waitAfterNextRender(wallpaperSubpageTopElement);

    // wallpaper selected page isn't displayed.
    const wallpaperSelected =
        wallpaperSubpageTopElement!.shadowRoot!.querySelector(
            'wallpaper-selected');
    assertFalse(
        !!wallpaperSelected,
        'wallpaper selected element should not be displayed.');

    // SeaPen input is displayed.
    const seaPenInputQueryElement =
        wallpaperSubpageTopElement.shadowRoot!.querySelector(
            'sea-pen-input-query');
    assertTrue(!!seaPenInputQueryElement, 'input query should be displayed.');

    // Template query should be hidden.
    const templateQuery = wallpaperSubpageTopElement.shadowRoot!.querySelector(
        'sea-pen-template-query');
    assertFalse(!!templateQuery, 'template query should not be displayed.');
  });

  test('displays input query tab', async () => {
    loadTimeData.overrideValues(
        {isSeaPenEnabled: true, isSeaPenTextInputEnabled: true});
    wallpaperSubpageTopElement = initElement(
        WallpaperSubpageTopElement, {path: Paths.SEA_PEN_COLLECTION});
    await waitAfterNextRender(wallpaperSubpageTopElement);
    const seaPenInputQueryElement =
        wallpaperSubpageTopElement.shadowRoot!.querySelector(
            'sea-pen-input-query');
    const inputQuery =
        seaPenInputQueryElement!.shadowRoot!.querySelector('cr-input');
    assertTrue(!!inputQuery, 'input query should display.');
    const searchButton = seaPenInputQueryElement!.shadowRoot!.querySelector(
                             '#searchButton') as HTMLElement;

    // Mock singleton |PersonalizationRouter|.
    const router = TestMock.fromClass(PersonalizationRouterElement);
    PersonalizationRouterElement.instance = () => router;

    // Mock |PersonalizationRouter.goToRoute()|.
    let selectedTemplateId: string|undefined;
    router.goToRoute = (path: string, queryParams: QueryParams) => {
      selectedTemplateId = queryParams.seaPenTemplateId;
      assertEquals(Paths.SEA_PEN_RESULTS, path);
    };

    // Make sure state starts at expected value.
    assertDeepEquals(emptyState(), personalizationStore.data);
    // Actually run the reducers.
    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(SeaPenActionName.SET_SEA_PEN_THUMBNAILS);

    // Update input query and click on search button.
    inputQuery.value = 'this is a test query';
    searchButton.click();

    assertEquals('Query', selectedTemplateId);

    await personalizationStore.waitForAction(
        SeaPenActionName.SET_SEA_PEN_THUMBNAILS);

    const expectedState: SeaPenState = {
      thumbnailsLoading: false,
      thumbnails: [
        {
          id: 1,
          image: {url: 'https://sea-pen-images.googleusercontent.com/1'},
        },
        {
          id: 2,
          image: {url: 'https://sea-pen-images.googleusercontent.com/2'},
        },
        {
          id: 3,
          image: {url: 'https://sea-pen-images.googleusercontent.com/3'},
        },
        {
          id: 4,
          image: {url: 'https://sea-pen-images.googleusercontent.com/4'},
        },
      ],
      recentImages: null,
      recentImageData: {},
    };
    assertDeepEquals(
        expectedState,
        personalizationStore.data.wallpaper.seaPen,
        'expected SeaPen state is set',
    );
  });

  test('displays template query content', async () => {
    loadTimeData.overrideValues({isSeaPenEnabled: true});
    // Initialize |wallpaperSubpageTopElement|.
    wallpaperSubpageTopElement = initElement(
        WallpaperSubpageTopElement,
        {path: Paths.SEA_PEN_COLLECTION, 'templateId': '4'});
    await waitAfterNextRender(wallpaperSubpageTopElement);

    const seaPenTemplateQueryElement =
        wallpaperSubpageTopElement.shadowRoot!.querySelector(
            'sea-pen-template-query');

    assertTrue(
        !!seaPenTemplateQueryElement, 'template query should be displayed.');
  });

  test('displays template query content on sea pen results page', async () => {
    loadTimeData.overrideValues({isSeaPenEnabled: true});
    // Initialize |wallpaperSubpageTopElement|.
    wallpaperSubpageTopElement = initElement(
        WallpaperSubpageTopElement,
        {path: Paths.SEA_PEN_RESULTS, 'templateId': '4'});
    await waitAfterNextRender(wallpaperSubpageTopElement);

    const seaPenTemplateQueryElement =
        wallpaperSubpageTopElement.shadowRoot!.querySelector(
            'sea-pen-template-query');

    assertTrue(
        !!seaPenTemplateQueryElement, 'template query should be displayed.');
  });

  test(
      'displays template query content when SeaPenTextInput is enabled',
      async () => {
        loadTimeData.overrideValues(
            {isSeaPenEnabled: true, isSeaPenTextInputEnabled: true});
        // Initialize |wallpaperSubpageTopElement|.
        wallpaperSubpageTopElement = initElement(
            WallpaperSubpageTopElement,
            {path: Paths.SEA_PEN_COLLECTION, 'templateId': '4'});
        await waitAfterNextRender(wallpaperSubpageTopElement);

        const seaPenTemplateQueryElement =
            wallpaperSubpageTopElement.shadowRoot!.querySelector(
                'sea-pen-template-query');

        assertTrue(
            !!seaPenTemplateQueryElement,
            'template query should be displayed.');
      });
});

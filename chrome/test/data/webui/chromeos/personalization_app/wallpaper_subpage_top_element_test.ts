// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {emptyState, Paths, PersonalizationRouterElement, WallpaperActionName, WallpaperSubpageTopElement} from 'chrome://personalization/js/personalization_app.js';
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
    assertTrue(!!seaPenInputQueryElement, 'input query should be display.');

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

    // Mock singleton |PersonalizationRouter|.
    const router = TestMock.fromClass(PersonalizationRouterElement);
    PersonalizationRouterElement.instance = () => router;

    // Mock |PersonalizationRouter.selectSeaPenTemplate()|.
    let selectedTemplateId: string|undefined;
    router.selectSeaPenTemplate = (templateId: string) => {
      selectedTemplateId = templateId;
    };

    // Make sure state starts at expected value.
    assertDeepEquals(emptyState(), personalizationStore.data);
    // Actually run the reducers.
    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(WallpaperActionName.SET_IMAGE_THUMBNAILS);

    // Update input query and click on search button.
    inputQuery.value = 'this is a test query';
    searchButton.click();

    assertEquals(selectedTemplateId, 'query');

    await personalizationStore.waitForAction(
        WallpaperActionName.SET_IMAGE_THUMBNAILS);

    assertDeepEquals(
        {
          query: 'this is a test query',
          thumbnailsLoading: false,
          thumbnails: [
            {
              id: BigInt(1),
              url: {url: 'chrome://personalization/images/feel_the_breeze.png'},
            },
            {
              id: BigInt(2),
              url: {url: 'chrome://personalization/images/float_on_by.png'},
            },
            {
              id: BigInt(3),
              url: {url: 'chrome://personalization/images/slideshow.png'},
            },
            {
              id: BigInt(4),
              url: {url: 'chrome://personalization/images/feel_the_breeze.png'},
            },
          ],
        },
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

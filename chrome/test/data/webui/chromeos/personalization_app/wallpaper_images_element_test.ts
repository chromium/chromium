// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {OnlineImageType, PersonalizationRouterElement, TimeOfDayWallpaperDialogElement, WallpaperGridItemElement, WallpaperImagesElement} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('WallpaperImagesElementTest', function() {
  let wallpaperImagesElement: WallpaperImagesElement|null;
  let wallpaperProvider: TestWallpaperProvider;
  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    const mocks = baseSetup();
    wallpaperProvider = mocks.wallpaperProvider;
    personalizationStore = mocks.personalizationStore;
  });

  teardown(async () => {
    await teardownElement(wallpaperImagesElement);
    wallpaperImagesElement = null;
  });

  async function createWithDefaultData(
      collectionId: string = wallpaperProvider.collections![0]!.id) {
    personalizationStore.data.wallpaper = {
      ...personalizationStore.data.wallpaper,
      backdrop: {
        collections: wallpaperProvider.collections,
        images: {[collectionId]: wallpaperProvider.images},
      },
      loading: {
        ...personalizationStore.data.wallpaper.loading,
        collections: false,
        images: {[collectionId]: false},
      },
      currentSelected: wallpaperProvider.currentWallpaper,
    };
    const element = initElement(WallpaperImagesElement, {collectionId});
    await waitAfterNextRender(element);
    return element;
  }

  async function selectTimeOfDayWallpaper() {
    // Click the first image that is not currently selected.
    wallpaperImagesElement!.shadowRoot!
        .querySelector<WallpaperGridItemElement>(`${
            WallpaperGridItemElement
                .is}[aria-selected='false'][data-is-time-of-day-wallpaper]`)!
        .click();
    await waitAfterNextRender(wallpaperImagesElement!);
    return wallpaperImagesElement;
  }

  async function clickTimeOfDayWallpaperDialogButton(id: string) {
    const dialog = wallpaperImagesElement!.shadowRoot!
                       .querySelector<TimeOfDayWallpaperDialogElement>(
                           TimeOfDayWallpaperDialogElement.is);
    assertNotEquals(null, dialog, 'dialog element must exist to click button');
    const button = dialog!.shadowRoot!.getElementById(id);
    assertNotEquals(null, button, `button with id ${id} must exist`);
    button!.click();
    await waitAfterNextRender(wallpaperImagesElement!);
    return wallpaperImagesElement;
  }

  test('sets aria-selected for current wallpaper asset id', async () => {
    wallpaperImagesElement = await createWithDefaultData();
    const selectedElements: WallpaperGridItemElement[] =
        Array.from(wallpaperImagesElement.shadowRoot!.querySelectorAll(
            `${WallpaperGridItemElement.is}[aria-selected='true']`));

    assertEquals(selectedElements.length, 1, '1 item aria selected');
    assertDeepEquals(
        selectedElements[0]!.src,
        [wallpaperProvider.images![0]!.url, wallpaperProvider.images![2]!.url],
        `item has correct src`);

    const notSelectedElements: HTMLDivElement[] =
        Array.from(wallpaperImagesElement.shadowRoot!.querySelectorAll(
            `${WallpaperGridItemElement.is}[aria-selected='false']`));

    const uniqueUnitIds =
        new Set(wallpaperProvider.images!.map(img => img.unitId));

    assertEquals(
        uniqueUnitIds.size - 1, notSelectedElements.length,
        'correct number of non-selected elements');
  });

  test('displays images for current collectionId', async () => {
    personalizationStore.data.wallpaper.backdrop.images = {
      'id_0': [
        {
          assetId: BigInt(1),
          attribution: ['Image 0-1'],
          type: OnlineImageType.kUnknown,
          unitId: BigInt(1),
          url: {url: 'https://id_0-1/'},
        },
        {
          assetId: BigInt(2),
          attribution: ['Image 0-2'],
          type: OnlineImageType.kUnknown,
          unitId: BigInt(2),
          url: {url: 'https://id_0-2/'},
        },
      ],
      'id_1': [
        {
          assetId: BigInt(10),
          attribution: ['Image 1-10'],
          type: OnlineImageType.kUnknown,
          unitId: BigInt(10),
          url: {url: 'https://id_1-10/'},
        },
        {
          assetId: BigInt(20),
          attribution: ['Image 1-20'],
          type: OnlineImageType.kUnknown,
          unitId: BigInt(20),
          url: {url: 'https://id_1-20/'},
        },
      ],
    };
    personalizationStore.data.wallpaper.backdrop.collections =
        wallpaperProvider.collections;
    personalizationStore.data.wallpaper.loading.images = {
      'id_0': false,
      'id_1': false,
    };
    personalizationStore.data.wallpaper.loading.collections = false;

    wallpaperImagesElement =
        initElement(WallpaperImagesElement, {collectionId: 'id_0'});
    await waitAfterNextRender(wallpaperImagesElement);

    assertDeepEquals(
        ['Image 0-1', 'Image 0-2'],
        Array
            .from(wallpaperImagesElement.shadowRoot!
                      .querySelectorAll<WallpaperGridItemElement>(
                          `${WallpaperGridItemElement.is}:not([hidden])`))
            .map(elem => elem.getAttribute('aria-label')),
        'expected aria labels are displayed for collectionId `id_0`');

    wallpaperImagesElement.collectionId = 'id_1';
    await waitAfterNextRender(wallpaperImagesElement);

    assertDeepEquals(
        ['Image 1-10', 'Image 1-20'],
        Array
            .from(wallpaperImagesElement.shadowRoot!
                      .querySelectorAll<WallpaperGridItemElement>(
                          `${WallpaperGridItemElement.is}:not([hidden])`))
            .map(elem => elem.getAttribute('aria-label')),
        'expected aria labels are displayed for collectionId `id_1`');
  });

  test('displays time of day tile for images with same unitId', async () => {
    personalizationStore.data.wallpaper.backdrop.images = {
      'id_0': [
        {
          assetId: BigInt(1),
          attribution: ['Light Image 0-1'],
          type: OnlineImageType.kLight,
          unitId: BigInt(1),
          url: {url: 'https://id_0-1/'},
        },
        {
          assetId: BigInt(2),
          attribution: ['Dark Image 0-2'],
          type: OnlineImageType.kDark,
          unitId: BigInt(1),
          url: {url: 'https://id_0-2/'},
        },
        {
          assetId: BigInt(3),
          attribution: ['Morning Image 0-3'],
          type: OnlineImageType.kMorning,
          unitId: BigInt(1),
          url: {url: 'https://id_0-3/'},
        },
        {
          assetId: BigInt(4),
          attribution: ['Late Afternoon Image 0-4'],
          type: OnlineImageType.kLateAfternoon,
          unitId: BigInt(1),
          url: {url: 'https://id_0-4/'},
        },
      ],
    };
    personalizationStore.data.wallpaper.backdrop.collections =
        wallpaperProvider.collections;
    personalizationStore.data.wallpaper.loading.images = {
      'id_0': false,
    };
    personalizationStore.data.wallpaper.loading.collections = false;

    wallpaperImagesElement =
        initElement(WallpaperImagesElement, {collectionId: 'id_0'});
    await waitAfterNextRender(wallpaperImagesElement);

    const elements =
        Array.from(wallpaperImagesElement.shadowRoot!
                       .querySelectorAll<WallpaperGridItemElement>(
                           `${WallpaperGridItemElement.is}:not([hidden])`));

    assertDeepEquals(
        [
          {url: 'https://id_0-1/'},
          {url: 'https://id_0-2/'},
          {url: 'https://id_0-3/'},
          {url: 'https://id_0-4/'},
        ],
        elements[0]!.src, 'time of day image has four variant urls');
    assertTrue(
        elements[0]!.hasAttribute('data-is-time-of-day-wallpaper'),
        'element has correct data attribute');
  });

  test(
      'displays one preview tile for images with the same unitId', async () => {
        personalizationStore.data.wallpaper.backdrop.images = {
          'id_0': [
            {
              assetId: BigInt(0),
              attribution: ['Late Afternoon Image 0-0'],
              type: OnlineImageType.kPreview,
              unitId: BigInt(1),
              url: {url: 'https://id_0-0/'},
            },
            {
              assetId: BigInt(1),
              attribution: ['Light Image 0-1'],
              type: OnlineImageType.kLight,
              unitId: BigInt(1),
              url: {url: 'https://id_0-1/'},
            },
            {
              assetId: BigInt(2),
              attribution: ['Dark Image 0-2'],
              type: OnlineImageType.kDark,
              unitId: BigInt(1),
              url: {url: 'https://id_0-2/'},
            },
            {
              assetId: BigInt(3),
              attribution: ['Morning Image 0-3'],
              type: OnlineImageType.kMorning,
              unitId: BigInt(1),
              url: {url: 'https://id_0-3/'},
            },
            {
              assetId: BigInt(4),
              attribution: ['Late Afternoon Image 0-4'],
              type: OnlineImageType.kLateAfternoon,
              unitId: BigInt(1),
              url: {url: 'https://id_0-4/'},
            },
          ],
        };
        personalizationStore.data.wallpaper.backdrop.collections =
            wallpaperProvider.collections;
        personalizationStore.data.wallpaper.loading.images = {
          'id_0': false,
        };
        personalizationStore.data.wallpaper.loading.collections = false;

        wallpaperImagesElement =
            initElement(WallpaperImagesElement, {collectionId: 'id_0'});
        await waitAfterNextRender(wallpaperImagesElement);

        const elements =
            Array.from(wallpaperImagesElement.shadowRoot!
                           .querySelectorAll<WallpaperGridItemElement>(
                               `${WallpaperGridItemElement.is}:not([hidden])`));

        assertDeepEquals(
            [
              {url: 'https://id_0-0/'},
            ],
            elements[0]!.src, 'preview image has only one url');
      });

  test('displays dark light tile for images with same unitId', async () => {
    wallpaperImagesElement = await createWithDefaultData();

    const elements =
        Array.from(wallpaperImagesElement.shadowRoot!
                       .querySelectorAll<WallpaperGridItemElement>(
                           `${WallpaperGridItemElement.is}:not([hidden])`));

    assertDeepEquals(
        ['Image 0 light', 'Image 2', 'Image 3'],
        elements.map(elem => elem.ariaLabel),
        'elements have correct aria labels for light mode');

    assertDeepEquals(
        [
          {url: 'https://images.googleusercontent.com/1'},
          {url: 'https://images.googleusercontent.com/0'},
        ],
        elements[0]?.src, 'dark/light mode image has dark light variant urls');

    assertEquals(
        OnlineImageType.kLight,
        wallpaperProvider.images!
            .find(image => image.url.url.endsWith('.com/1'))!.type,
        'light image is first');

    assertDeepEquals(
        [{url: 'https://images.googleusercontent.com/2'}], elements[1]?.src,
        'image 2 does not have dark light mode variants');
  });

  test('selects an image when clicked', async () => {
    wallpaperImagesElement = await createWithDefaultData();
    // Click the first image that is not currently selected.
    wallpaperImagesElement.shadowRoot!
        .querySelector<WallpaperGridItemElement>(
            `${WallpaperGridItemElement.is}[aria-selected='false']`)!.click();
    const [assetId, previewMode] =
        await wallpaperProvider.whenCalled('selectWallpaper');
    assertEquals(2n, assetId, 'correct asset id is passed');
    assertEquals(
        wallpaperProvider.isInTabletModeResponse, previewMode,
        'preview mode is same as tablet mode');
    assertEquals(
        null,
        wallpaperImagesElement.shadowRoot!.querySelector(
            TimeOfDayWallpaperDialogElement.is),
        'no time of day dialog when selecting a regular image');
  });

  test('shows dialog when clicking on a time of day wallpaper', async () => {
    loadTimeData.overrideValues({
      isTimeOfDayWallpaperEnabled: true,
    });
    personalizationStore.setReducersEnabled(true);
    personalizationStore.data.theme.colorModeAutoScheduleEnabled = false;
    wallpaperImagesElement =
        await createWithDefaultData(wallpaperProvider.timeOfDayCollectionId);

    await selectTimeOfDayWallpaper();
    assertNotEquals(
        null,
        wallpaperImagesElement.shadowRoot!.querySelector(
            TimeOfDayWallpaperDialogElement.is),
        'dialog element exists');
  });

  test(
      'clicking cancel dismisses the time of day wallpaper dialog',
      async () => {
        loadTimeData.overrideValues({
          isTimeOfDayWallpaperEnabled: true,
        });
        personalizationStore.setReducersEnabled(true);
        personalizationStore.data.theme.colorModeAutoScheduleEnabled = false;
        wallpaperImagesElement = await createWithDefaultData(
            wallpaperProvider.timeOfDayCollectionId);

        await selectTimeOfDayWallpaper();
        await clickTimeOfDayWallpaperDialogButton('close');
        assertEquals(
            null,
            wallpaperImagesElement.shadowRoot!.querySelector(
                TimeOfDayWallpaperDialogElement.is),
            'clicking cancel dismisses the dialog');
        const [assetId, previewMode] =
            await wallpaperProvider.whenCalled('selectWallpaper');
        assertEquals(3n, assetId, 'correct asset id is passed');
        assertEquals(
            wallpaperProvider.isInTabletModeResponse, previewMode,
            'preview mode is same as tablet mode');
        assertFalse(
            personalizationStore.data.theme.colorModeAutoScheduleEnabled,
            'auto dark mode is not enabled');
      });

  test('clicking confirm on the time of day wallpaper dialog', async () => {
    loadTimeData.overrideValues({
      isTimeOfDayWallpaperEnabled: true,
    });
    personalizationStore.setReducersEnabled(true);
    personalizationStore.data.theme.colorModeAutoScheduleEnabled = false;
    wallpaperImagesElement =
        await createWithDefaultData(wallpaperProvider.timeOfDayCollectionId);

    await selectTimeOfDayWallpaper();
    await clickTimeOfDayWallpaperDialogButton('accept');
    assertEquals(
        null,
        wallpaperImagesElement.shadowRoot!.querySelector(
            TimeOfDayWallpaperDialogElement.is),
        'clicking accept dismisses the dialog');
    const [assetId, previewMode] =
        await wallpaperProvider.whenCalled('selectWallpaper');
    assertEquals(3n, assetId, 'correct asset id is passed');
    assertEquals(
        wallpaperProvider.isInTabletModeResponse, previewMode,
        'preview mode is same as tablet mode');
    assertTrue(
        personalizationStore.data.theme.colorModeAutoScheduleEnabled,
        'auto dark mode is enabled');
  });

  test('do not show time of day dialog with proper settings', async () => {
    loadTimeData.overrideValues({
      isTimeOfDayWallpaperEnabled: true,
    });
    personalizationStore.setReducersEnabled(true);
    wallpaperProvider.shouldShowTimeOfDayWallpaperDialogResponse = false;
    wallpaperImagesElement =
        await createWithDefaultData(wallpaperProvider.timeOfDayCollectionId);
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperImagesElement);

    await selectTimeOfDayWallpaper();
    assertEquals(
        null,
        wallpaperImagesElement.shadowRoot!.querySelector(
            TimeOfDayWallpaperDialogElement.is),
        'dialog element does not exist');
    const [assetId, _] = await wallpaperProvider.whenCalled('selectWallpaper');
    assertEquals(3n, assetId, 'correct asset id is passed');
  });

  test('dismiss time of day promo banner after showing images', async () => {
    personalizationStore.setReducersEnabled(true);
    personalizationStore.data.ambient.shouldShowTimeOfDayBanner = true;
    wallpaperImagesElement =
        await createWithDefaultData(wallpaperProvider.timeOfDayCollectionId);

    assertFalse(
        personalizationStore.data.ambient.shouldShowTimeOfDayBanner,
        'banner is dismissed');
  });

  test('redirects to wallpaper page if no images', async () => {
    const reloadOriginal = PersonalizationRouterElement.reloadAtWallpaper;
    const reloadPromise = new Promise<void>(resolve => {
      PersonalizationRouterElement.reloadAtWallpaper = resolve;
    });
    const collectionId = wallpaperProvider.collections![0]!.id;
    // Set all collections to have null images.
    personalizationStore.data.wallpaper = {
      ...personalizationStore.data.wallpaper,
      backdrop: {
        collections: wallpaperProvider.collections,
        images: {[collectionId]: null},
      },
      loading: {
        ...personalizationStore.data.wallpaper.loading,
        collections: false,
        images: {[collectionId]: false},
      },
    };
    wallpaperImagesElement =
        initElement(WallpaperImagesElement, {collectionId});

    await reloadPromise;

    PersonalizationRouterElement.reloadAtWallpaper = reloadOriginal;
  });
});

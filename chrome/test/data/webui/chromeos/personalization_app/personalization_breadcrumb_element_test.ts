// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for wallpaper-breadcrumb component.  */

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {GooglePhotosAlbum, Paths, PersonalizationBreadcrumb, PersonalizationRouter, TopicSource} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('PersonalizationBreadcrumbTest', function() {
  let breadcrumbElement: PersonalizationBreadcrumb|null;

  let wallpaperProvider: TestWallpaperProvider;

  let personalizationStore: TestPersonalizationStore;

  /**
   * Asserts that the specified |breadcrumbContainer| has children conforming
   * to the specified |breadcrumbs|, e.g. ["Wallpaper", "Google Photos"].
   */
  function assertBreadcrumbs(
      breadcrumbContainer: HTMLElement, breadcrumbs: string[]) {
    // Ignore child elements which are not breadcrumbs.
    const breadcrumbEls = [...breadcrumbContainer.children].filter(child => {
      return child.classList.contains('breadcrumb');
    });

    assertEquals(breadcrumbEls.length, breadcrumbs.length);

    for (let i = 0; i < breadcrumbs.length; ++i) {
      const breadcrumb = breadcrumbs[i];
      const breadcrumbEl = breadcrumbEls[i];

      assertEquals(breadcrumbEl!.textContent, breadcrumb);

      if (i < breadcrumbs.length - 1) {
        // Breadcrumbs are separated by a chevron icon.
        const chevronEl = breadcrumbEl!.nextElementSibling;
        assertEquals(chevronEl!.tagName, 'IRON-ICON');
        assertEquals(chevronEl!.getAttribute('icon'), 'cr:chevron-right');
      }
    }
  }

  setup(() => {
    const mocks = baseSetup();
    wallpaperProvider = mocks.wallpaperProvider;
    personalizationStore = mocks.personalizationStore;
  });

  teardown(async () => {
    if (breadcrumbElement) {
      breadcrumbElement.remove();
    }
    breadcrumbElement = null;
    await flushTasks();
  });

  test('show label when wallpaper subpage is loaded', async () => {
    breadcrumbElement =
        initElement(PersonalizationBreadcrumb, {'path': Paths.COLLECTIONS});

    await waitAfterNextRender(breadcrumbElement);

    let breadcrumbContainer =
        breadcrumbElement.shadowRoot!.getElementById('selector');
    assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
    assertBreadcrumbs(
        breadcrumbContainer!, [breadcrumbElement.i18n('wallpaperLabel')]);

    // current breadcrumbs Home > Wallpaper.
    // remain in the same page when Wallpaper is clicked on.
    const wallpaperBreadcrumb =
        breadcrumbElement.shadowRoot!.getElementById('breadcrumb0');
    wallpaperBreadcrumb!.click();

    breadcrumbContainer =
        breadcrumbElement.shadowRoot!.getElementById('selector');
    assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
    assertBreadcrumbs(
        breadcrumbContainer!, [breadcrumbElement.i18n('wallpaperLabel')]);
  });

  test('click home button goes back to root page', async () => {
    breadcrumbElement =
        initElement(PersonalizationBreadcrumb, {'path': Paths.COLLECTIONS});
    await waitAfterNextRender(breadcrumbElement);

    // navigate to main page when Home icon is clicked on.
    const original = PersonalizationRouter.instance;
    const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
      PersonalizationRouter.instance = () => {
        return {
          goToRoute(path: Paths, queryParams: Object = {}) {
            resolve([path, queryParams]);
            PersonalizationRouter.instance = original;
          },
        } as PersonalizationRouter;
      };
    });

    const homeButton =
        breadcrumbElement!.shadowRoot!.getElementById('homeButton');
    homeButton!.click();
    const [path, queryParams] = await goToRoutePromise;
    assertEquals(Paths.ROOT, path);
    assertDeepEquals({}, queryParams);
  });

  test('back button hidden if personalization hub feature is on', async () => {
    breadcrumbElement =
        initElement(PersonalizationBreadcrumb, {'path': Paths.COLLECTIONS});
    await waitAfterNextRender(breadcrumbElement);

    assertTrue(
        !breadcrumbElement!.shadowRoot!.getElementById('backButton'),
        'no back button');
  });

  test('shows collection name when collection is selected', async () => {
    const collection = wallpaperProvider.collections![0];
    assertTrue(!!collection);
    breadcrumbElement = initElement(
        PersonalizationBreadcrumb,
        {'path': Paths.COLLECTION_IMAGES, 'collectionId': collection.id});

    personalizationStore.data.wallpaper.backdrop.collections =
        wallpaperProvider.collections;
    personalizationStore.notifyObservers();

    await waitAfterNextRender(breadcrumbElement);

    const breadcrumbContainer =
        breadcrumbElement.shadowRoot!.getElementById('selector');
    assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
    assertBreadcrumbs(
        breadcrumbContainer!,
        [breadcrumbElement.i18n('wallpaperLabel'), collection!.name]);

    const original = PersonalizationRouter.instance;
    const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
      PersonalizationRouter.instance = () => {
        return {
          goToRoute(path: Paths, queryParams: Object = {}) {
            resolve([path, queryParams]);
            PersonalizationRouter.instance = original;
          },
        } as PersonalizationRouter;
      };
    });

    // current breadcrumbs: Home > Wallpaper > Zero
    // navigate to Wallpaper subpage when Wallpaper breadcrumb is clicked on.
    const wallpaperBreadcrumb =
        breadcrumbElement!.shadowRoot!.getElementById('breadcrumb0');
    wallpaperBreadcrumb!.click();
    const [path, queryParams] = await goToRoutePromise;
    assertEquals(Paths.COLLECTIONS, path);
    assertDeepEquals({}, queryParams);
  });

  test('show album name when Google Photos subpage is loaded', async () => {
    // The `googlePhotosLabel` string is only supplied when the Google Photos
    // integration feature flag is enabled.
    loadTimeData.overrideValues({'googlePhotosLabel': 'Google Photos'});

    const googlePhotosAlbum = new GooglePhotosAlbum();
    googlePhotosAlbum.id = '9bd1d7a3-f995-4445-be47-53c5b58ce1cb';
    googlePhotosAlbum.title = 'Album 0';

    personalizationStore.data.wallpaper.googlePhotos.albums =
        [googlePhotosAlbum];
    personalizationStore.notifyObservers();

    breadcrumbElement = initElement(PersonalizationBreadcrumb, {
      'path': Paths.GOOGLE_PHOTOS_COLLECTION,
      'googlePhotosAlbumId': googlePhotosAlbum.id,
    });

    const breadcrumbContainer =
        breadcrumbElement.shadowRoot!.getElementById('selector');
    assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
    assertBreadcrumbs(breadcrumbContainer, [
      breadcrumbElement.i18n('wallpaperLabel'),
      breadcrumbElement.i18n('googlePhotosLabel'),
      googlePhotosAlbum.title,
    ]);

    const original = PersonalizationRouter.instance;
    const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
      PersonalizationRouter.instance = () => {
        return {
          goToRoute(path: Paths, queryParams: Object = {}) {
            resolve([path, queryParams]);
            PersonalizationRouter.instance = original;
          },
        } as PersonalizationRouter;
      };
    });

    // current breadcrumbs: Home > Wallpaper > Google Photos > Album 0
    // navigate to Google Photos subpage when Google Photos breadcrumb is
    // clicked on.
    const googlePhotoBreadcrumb =
        breadcrumbElement!.shadowRoot!.getElementById('breadcrumb1');
    googlePhotoBreadcrumb!.click();
    const [path, queryParams] = await goToRoutePromise;
    assertEquals(Paths.GOOGLE_PHOTOS_COLLECTION, path);
    assertDeepEquals({}, queryParams);
  });

  test('show label when Google Photos subpage is loaded', async () => {
    // The `googlePhotosLabel` string is only supplied when the Google Photos
    // integration feature flag is enabled.
    loadTimeData.overrideValues({'googlePhotosLabel': 'Google Photos'});

    breadcrumbElement = initElement(
        PersonalizationBreadcrumb, {'path': Paths.GOOGLE_PHOTOS_COLLECTION});

    const breadcrumbContainer =
        breadcrumbElement.shadowRoot!.getElementById('selector');
    assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
    assertBreadcrumbs(breadcrumbContainer, [
      breadcrumbElement.i18n('wallpaperLabel'),
      breadcrumbElement.i18n('googlePhotosLabel'),
    ]);

    const original = PersonalizationRouter.instance;
    const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
      PersonalizationRouter.instance = () => {
        return {
          goToRoute(path: Paths, queryParams: Object = {}) {
            resolve([path, queryParams]);
            PersonalizationRouter.instance = original;
          },
        } as PersonalizationRouter;
      };
    });

    // current breadcrumbs: Home > Wallpaper > Google Photos
    // navigate to Wallpaper subpage when Wallpaper breadcrumb is clicked on.
    const wallpaperBreadcrumb =
        breadcrumbElement!.shadowRoot!.getElementById('breadcrumb0');
    wallpaperBreadcrumb!.click();
    const [path, queryParams] = await goToRoutePromise;
    assertEquals(Paths.COLLECTIONS, path);
    assertDeepEquals({}, queryParams);
  });

  test('show label when local images subpage is loaded', async () => {
    breadcrumbElement = initElement(
        PersonalizationBreadcrumb, {'path': Paths.LOCAL_COLLECTION});

    personalizationStore.data.wallpaper.local.images =
        wallpaperProvider.localImages;
    personalizationStore.notifyObservers();

    await waitAfterNextRender(breadcrumbElement);

    const breadcrumbContainer =
        breadcrumbElement.shadowRoot!.getElementById('selector');
    assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
    assertBreadcrumbs(breadcrumbContainer, [
      breadcrumbElement.i18n('wallpaperLabel'),
      breadcrumbElement.i18n('myImagesLabel'),
    ]);

    const original = PersonalizationRouter.instance;
    const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
      PersonalizationRouter.instance = () => {
        return {
          goToRoute(path: Paths, queryParams: Object = {}) {
            resolve([path, queryParams]);
            PersonalizationRouter.instance = original;
          },
        } as PersonalizationRouter;
      };
    });

    // current breadcrumbs: Home > Wallpaper > My Images
    // navigate to Wallpaper subpage when Wallpaper breadcrumb is clicked on.
    const wallpaperBreadcrumb =
        breadcrumbElement!.shadowRoot!.getElementById('breadcrumb0');
    wallpaperBreadcrumb!.click();
    const [path, queryParams] = await goToRoutePromise;
    assertEquals(Paths.COLLECTIONS, path);
    assertDeepEquals({}, queryParams);
  });

  test('show label when ambient subpage is loaded', async () => {
    breadcrumbElement =
        initElement(PersonalizationBreadcrumb, {'path': Paths.AMBIENT});

    await waitAfterNextRender(breadcrumbElement);

    let breadcrumbContainer =
        breadcrumbElement.shadowRoot!.getElementById('selector');
    assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
    assertBreadcrumbs(
        breadcrumbContainer!, [breadcrumbElement.i18n('screensaverLabel')]);

    // current breadcrumbs Home > Screensaver.
    // remain in the same page when Screensaver is clicked on.
    const screensaverBreadcrumb =
        breadcrumbElement.shadowRoot!.getElementById('breadcrumb0');
    screensaverBreadcrumb!.click();

    breadcrumbContainer =
        breadcrumbElement.shadowRoot!.getElementById('selector');
    assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
    assertBreadcrumbs(
        breadcrumbContainer!, [breadcrumbElement.i18n('screensaverLabel')]);
  });

  test(
      'show label when ambient album page - Google Photos is loaded',
      async () => {
        loadTimeData.overrideValues(
            {'ambientModeTopicSourceGooglePhotos': 'Google Photos'});

        breadcrumbElement = initElement(PersonalizationBreadcrumb, {
          'path': Paths.AMBIENT_ALBUMS,
          'topicSource': TopicSource.kGooglePhotos,
        });

        const breadcrumbContainer =
            breadcrumbElement.shadowRoot!.getElementById('selector');
        assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
        assertBreadcrumbs(breadcrumbContainer, [
          breadcrumbElement.i18n('screensaverLabel'),
          breadcrumbElement.i18n('ambientModeTopicSourceGooglePhotos'),
        ]);

        const original = PersonalizationRouter.instance;
        const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
          PersonalizationRouter.instance = () => {
            return {
              goToRoute(path: Paths, queryParams: Object = {}) {
                resolve([path, queryParams]);
                PersonalizationRouter.instance = original;
              },
            } as PersonalizationRouter;
          };
        });

        // current breadcrumbs: Home > Screensaver > Google Photos
        // navigate to ambient subpage when Screensaver breadcrumb is clicked
        // on.
        const screensaverBreadcrumb =
            breadcrumbElement!.shadowRoot!.getElementById('breadcrumb0');
        screensaverBreadcrumb!.click();
        const [path, queryParams] = await goToRoutePromise;
        assertEquals(Paths.AMBIENT, path);
        assertDeepEquals({}, queryParams);
      });

  test(
      'show label when ambient album page - Art gallery is loaded',
      async () => {
        loadTimeData.overrideValues(
            {'ambientModeTopicSourceArtGallery': 'Art Gallery'});

        breadcrumbElement = initElement(PersonalizationBreadcrumb, {
          'path': Paths.AMBIENT_ALBUMS,
          'topicSource': TopicSource.kArtGallery,
        });

        const breadcrumbContainer =
            breadcrumbElement.shadowRoot!.getElementById('selector');
        assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
        assertBreadcrumbs(breadcrumbContainer, [
          breadcrumbElement.i18n('screensaverLabel'),
          breadcrumbElement.i18n('ambientModeTopicSourceArtGallery'),
        ]);

        const original = PersonalizationRouter.instance;
        const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
          PersonalizationRouter.instance = () => {
            return {
              goToRoute(path: Paths, queryParams: Object = {}) {
                resolve([path, queryParams]);
                PersonalizationRouter.instance = original;
              },
            } as PersonalizationRouter;
          };
        });

        // current breadcrumbs: Home > Screensaver > Art Gallery
        // navigate to ambient subpage when Screensaver breadcrumb is clicked
        // on.
        const screensaverBreadcrumb =
            breadcrumbElement!.shadowRoot!.getElementById('breadcrumb0');
        screensaverBreadcrumb!.click();
        const [path, queryParams] = await goToRoutePromise;
        assertEquals(Paths.AMBIENT, path);
        assertDeepEquals({}, queryParams);
      });
});

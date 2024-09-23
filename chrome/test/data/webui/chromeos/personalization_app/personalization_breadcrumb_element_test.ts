// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for wallpaper-breadcrumb component.  */

import 'chrome://personalization/strings.m.js';

import {GooglePhotosAlbum, Paths, PersonalizationBreadcrumbElement, PersonalizationRouterElement, TopicSource} from 'chrome://personalization/js/personalization_app.js';
import {SeaPenTemplateId} from 'chrome://resources/ash/common/sea_pen/sea_pen_generated.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('PersonalizationBreadcrumbElementTest', function() {
  let breadcrumbElement: PersonalizationBreadcrumbElement|null;

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
        let nextSiblingEl = breadcrumbEl!.nextElementSibling;
        while (nextSiblingEl &&
               (nextSiblingEl as HTMLElement).style.display === 'none') {
          nextSiblingEl = nextSiblingEl.nextElementSibling;
        }
        // The first visible sibling should be a chevron icon. Breadcrumbs are
        // separated by a chevron icon.
        assertEquals(nextSiblingEl!.tagName, 'IRON-ICON');
        assertEquals(nextSiblingEl!.getAttribute('icon'), 'cr:chevron-right');
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
    breadcrumbElement = initElement(
        PersonalizationBreadcrumbElement, {'path': Paths.COLLECTIONS});

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
    breadcrumbElement = initElement(
        PersonalizationBreadcrumbElement, {'path': Paths.COLLECTIONS});
    await waitAfterNextRender(breadcrumbElement);

    // navigate to main page when Home icon is clicked on.
    const original = PersonalizationRouterElement.instance;
    const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
      PersonalizationRouterElement.instance = () => {
        return {
          goToRoute(path: Paths, queryParams: Object = {}) {
            resolve([path, queryParams]);
            PersonalizationRouterElement.instance = original;
          },
        } as PersonalizationRouterElement;
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
    breadcrumbElement = initElement(
        PersonalizationBreadcrumbElement, {'path': Paths.COLLECTIONS});
    await waitAfterNextRender(breadcrumbElement);

    assertTrue(
        !breadcrumbElement!.shadowRoot!.getElementById('backButton'),
        'no back button');
  });

  test('shows collection name when collection is selected', async () => {
    const collection = wallpaperProvider.collections![0];
    assertTrue(!!collection);
    breadcrumbElement = initElement(
        PersonalizationBreadcrumbElement,
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

    const original = PersonalizationRouterElement.instance;
    const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
      PersonalizationRouterElement.instance = () => {
        return {
          goToRoute(path: Paths, queryParams: Object = {}) {
            resolve([path, queryParams]);
            PersonalizationRouterElement.instance = original;
          },
        } as PersonalizationRouterElement;
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

    const googlePhotosAlbum: GooglePhotosAlbum = {
      id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
      title: 'Album 0',
      photoCount: 0,
      isShared: false,
      preview: {url: ''},
      timestamp: {internalValue: BigInt(0)},
    };

    personalizationStore.data.wallpaper.googlePhotos.albums =
        [googlePhotosAlbum];
    personalizationStore.notifyObservers();

    breadcrumbElement = initElement(PersonalizationBreadcrumbElement, {
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

    const original = PersonalizationRouterElement.instance;
    const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
      PersonalizationRouterElement.instance = () => {
        return {
          goToRoute(path: Paths, queryParams: Object = {}) {
            resolve([path, queryParams]);
            PersonalizationRouterElement.instance = original;
          },
        } as PersonalizationRouterElement;
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
        PersonalizationBreadcrumbElement,
        {'path': Paths.GOOGLE_PHOTOS_COLLECTION});

    const breadcrumbContainer =
        breadcrumbElement.shadowRoot!.getElementById('selector');
    assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
    assertBreadcrumbs(breadcrumbContainer, [
      breadcrumbElement.i18n('wallpaperLabel'),
      breadcrumbElement.i18n('googlePhotosLabel'),
    ]);

    const original = PersonalizationRouterElement.instance;
    const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
      PersonalizationRouterElement.instance = () => {
        return {
          goToRoute(path: Paths, queryParams: Object = {}) {
            resolve([path, queryParams]);
            PersonalizationRouterElement.instance = original;
          },
        } as PersonalizationRouterElement;
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
        PersonalizationBreadcrumbElement, {'path': Paths.LOCAL_COLLECTION});

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

    const original = PersonalizationRouterElement.instance;
    const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
      PersonalizationRouterElement.instance = () => {
        return {
          goToRoute(path: Paths, queryParams: Object = {}) {
            resolve([path, queryParams]);
            PersonalizationRouterElement.instance = original;
          },
        } as PersonalizationRouterElement;
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
        initElement(PersonalizationBreadcrumbElement, {'path': Paths.AMBIENT});

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

        breadcrumbElement = initElement(PersonalizationBreadcrumbElement, {
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

        const original = PersonalizationRouterElement.instance;
        const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
          PersonalizationRouterElement.instance = () => {
            return {
              goToRoute(path: Paths, queryParams: Object = {}) {
                resolve([path, queryParams]);
                PersonalizationRouterElement.instance = original;
              },
            } as PersonalizationRouterElement;
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

        breadcrumbElement = initElement(PersonalizationBreadcrumbElement, {
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

        const original = PersonalizationRouterElement.instance;
        const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
          PersonalizationRouterElement.instance = () => {
            return {
              goToRoute(path: Paths, queryParams: Object = {}) {
                resolve([path, queryParams]);
                PersonalizationRouterElement.instance = original;
              },
            } as PersonalizationRouterElement;
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

  test('show breadcrumbs for SeaPen templates', async () => {
    loadTimeData.overrideValues({isSeaPenTextInputEnabled: false});
    breadcrumbElement = initElement(PersonalizationBreadcrumbElement, {
      'path': Paths.SEA_PEN_COLLECTION,
    });

    const breadcrumbContainer =
        breadcrumbElement.shadowRoot!.getElementById('selector');
    assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
    assertBreadcrumbs(breadcrumbContainer, [
      breadcrumbElement.i18n('wallpaperLabel'),
      breadcrumbElement.i18n('seaPenLabel'),
    ]);
  });

  test(
      'show breadcrumbs for SeaPen templates with freeform enabled',
      async () => {
        loadTimeData.overrideValues({isSeaPenTextInputEnabled: true});
        breadcrumbElement = initElement(PersonalizationBreadcrumbElement, {
          'path': Paths.SEA_PEN_COLLECTION,
        });

        const breadcrumbContainer =
            breadcrumbElement.shadowRoot!.getElementById('selector');
        assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
        assertBreadcrumbs(breadcrumbContainer, [
          breadcrumbElement.i18n('wallpaperLabel'),
          breadcrumbElement.i18n('seaPenFreeformWallpaperTemplatesLabel'),
        ]);
      });

  test('show breadcrumbs for SeaPen results content', async () => {
    loadTimeData.overrideValues({isSeaPenTextInputEnabled: false});
    breadcrumbElement = initElement(PersonalizationBreadcrumbElement, {
      'path': Paths.SEA_PEN_RESULTS,
      'seaPenTemplateId': SeaPenTemplateId.kFlower.toString(),
    });

    const breadcrumbContainer =
        breadcrumbElement.shadowRoot!.getElementById('selector');
    assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
    assertBreadcrumbs(breadcrumbContainer, [
      breadcrumbElement.i18n('wallpaperLabel'),
      breadcrumbElement.i18n('seaPenLabel'),
      'Airbrushed',
    ]);

    const original = PersonalizationRouterElement.instance;
    const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
      PersonalizationRouterElement.instance = () => {
        return {
          goToRoute(path: Paths, queryParams: Object = {}) {
            resolve([path, queryParams]);
            PersonalizationRouterElement.instance = original;
          },
        } as PersonalizationRouterElement;
      };
    });

    // current breadcrumbs: Home > Wallpaper > Sea Pen > Park
    // navigate to Sea Pen subpage when Sea Pen breadcrumb is clicked on.
    const seaPenBreadcrumb =
        breadcrumbElement!.shadowRoot!.getElementById('breadcrumb1');
    seaPenBreadcrumb!.click();
    const [path, queryParams] = await goToRoutePromise;
    assertEquals(Paths.SEA_PEN_COLLECTION, path);
    assertDeepEquals({}, queryParams);
  });

  test(
      'show breadcrumbs for SeaPen results content with freeform enabled',
      async () => {
        loadTimeData.overrideValues({isSeaPenTextInputEnabled: true});
        breadcrumbElement = initElement(PersonalizationBreadcrumbElement, {
          'path': Paths.SEA_PEN_RESULTS,
          'seaPenTemplateId': SeaPenTemplateId.kFlower.toString(),
        });

        const breadcrumbContainer =
            breadcrumbElement.shadowRoot!.getElementById('selector');
        assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
        assertBreadcrumbs(breadcrumbContainer, [
          breadcrumbElement.i18n('wallpaperLabel'),
          breadcrumbElement.i18n('seaPenFreeformWallpaperTemplatesLabel'),
          'Airbrushed',
        ]);

        const original = PersonalizationRouterElement.instance;
        const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
          PersonalizationRouterElement.instance = () => {
            return {
              goToRoute(path: Paths, queryParams: Object = {}) {
                resolve([path, queryParams]);
                PersonalizationRouterElement.instance = original;
              },
            } as PersonalizationRouterElement;
          };
        });

        // current breadcrumbs: Home > Wallpaper > Sea Pen > Park
        // navigate to Sea Pen subpage when Sea Pen breadcrumb is clicked on.
        const seaPenBreadcrumb =
            breadcrumbElement!.shadowRoot!.getElementById('breadcrumb1');
        seaPenBreadcrumb!.click();
        const [path, queryParams] = await goToRoutePromise;
        assertEquals(Paths.SEA_PEN_COLLECTION, path);
        assertDeepEquals({}, queryParams);
      });

  test('show breadcrumbs for SeaPen freeform', async () => {
    breadcrumbElement = initElement(PersonalizationBreadcrumbElement, {
      'path': Paths.SEA_PEN_FREEFORM,
    });

    const breadcrumbContainer =
        breadcrumbElement.shadowRoot!.getElementById('selector');
    assertTrue(!!breadcrumbContainer && !breadcrumbContainer.hidden);
    assertBreadcrumbs(breadcrumbContainer, [
      breadcrumbElement.i18n('wallpaperLabel'),
      breadcrumbElement.i18n('seaPenLabel'),
    ]);
  });

  test('hide dropdown icon for sea pen templates', async () => {
    loadTimeData.overrideValues({isSeaPenEnabled: true});
    breadcrumbElement = initElement(PersonalizationBreadcrumbElement, {
      'path': Paths.SEA_PEN_COLLECTION,
    });

    const dropdownIcon =
        breadcrumbElement.shadowRoot!.querySelector('#seaPenDropdown');

    assertFalse(!!dropdownIcon);
  });

  test('show dropdown icon for SeaPen results', async () => {
    loadTimeData.overrideValues({isSeaPenEnabled: true});
    breadcrumbElement = initElement(PersonalizationBreadcrumbElement, {
      'path': Paths.SEA_PEN_RESULTS,
      'seaPenTemplateId': SeaPenTemplateId.kFlower.toString(),
    });

    const dropdownIcon =
        breadcrumbElement.shadowRoot!.querySelector('#seaPenDropdown');

    assertTrue(!!dropdownIcon);
  });

  test('click SeaPen template breadcrumb to show dropdown menu', async () => {
    loadTimeData.overrideValues({isSeaPenEnabled: true});
    breadcrumbElement = initElement(PersonalizationBreadcrumbElement, {
      'path': Paths.SEA_PEN_RESULTS,
      'seaPenTemplateId': SeaPenTemplateId.kFlower.toString(),
    });

    const breadcrumb = (breadcrumbElement.shadowRoot!.querySelector(
                            '#seaPenDropdown') as HTMLElement)
                           .parentElement;
    breadcrumb!.click();

    const dropdownMenu =
        breadcrumbElement.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!dropdownMenu);
    assertTrue(dropdownMenu!.open, 'the action menu should be open');
    const allMenuItems = dropdownMenu.querySelectorAll('button');
    assertTrue(allMenuItems.length > 1);
    const selectedElement =
        dropdownMenu.querySelectorAll('button[aria-checked=\'true\']');
    assertEquals(1, selectedElement.length);
    assertEquals('Airbrushed', (selectedElement[0] as HTMLElement)!.innerText);
  });

  test('navigates with SeaPen dropdown', async () => {
    loadTimeData.overrideValues({isSeaPenEnabled: true});
    breadcrumbElement = initElement(PersonalizationBreadcrumbElement, {
      'path': Paths.SEA_PEN_RESULTS,
      'seaPenTemplateId': SeaPenTemplateId.kFlower.toString(),
    });
    const dropdownIcon = breadcrumbElement.shadowRoot!.querySelector(
                             '#seaPenDropdown') as HTMLElement;
    dropdownIcon!.click();
    const dropdownMenu =
        breadcrumbElement.shadowRoot!.querySelector('cr-action-menu');
    const template =
        (dropdownMenu!.querySelectorAll('button[aria-checked=\'false\']')[0] as
         HTMLElement);

    const original = PersonalizationRouterElement.instance;
    const goToRoutePromise = new Promise<[Paths, Object]>(resolve => {
      PersonalizationRouterElement.instance = () => {
        return {
          goToRoute(path: Paths, queryParams: Object = {}) {
            resolve([path, queryParams]);
            PersonalizationRouterElement.instance = original;
          },
        } as PersonalizationRouterElement;
      };
    });

    template!.click();

    const [path, queryParams] = await goToRoutePromise;
    assertEquals(Paths.SEA_PEN_RESULTS, path);
    assertDeepEquals({'seaPenTemplateId': template.dataset['id']}, queryParams);
    assertFalse(
        !!breadcrumbElement.shadowRoot!.querySelector('cr-action-menu')?.open,
        'the action menu should be closed');
  });

  function pressLeftKey(el: HTMLElement) {
    el.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'left',
      bubbles: true,
      composed: true,
    }));
  }

  test('resets tabindex if the focusable breadcrumb is removed', async () => {
    loadTimeData.overrideValues({isSeaPenEnabled: true});
    breadcrumbElement = initElement(PersonalizationBreadcrumbElement, {
      'path': Paths.SEA_PEN_RESULTS,
      'seaPenTemplateId': SeaPenTemplateId.kFlower.toString(),
    });

    await waitAfterNextRender(breadcrumbElement);

    // Get all 4 breadcrumbs.
    let allBreadcrumbs =
        Array.from(breadcrumbElement.shadowRoot!.querySelectorAll(
            '.selectable')) as HTMLElement[];
    assertEquals(4, allBreadcrumbs.length);

    // Check initial tab indices.
    assertEquals('0', allBreadcrumbs[0]!.getAttribute('tabindex'));
    assertNotEquals('0', allBreadcrumbs[1]!.getAttribute('tabindex'));
    assertNotEquals('0', allBreadcrumbs[2]!.getAttribute('tabindex'));
    // The last breadcrumb which is a button to open the template menu dropdown
    // list should have tabindex 0.
    assertEquals('0', allBreadcrumbs[3]!.getAttribute('tabindex'));

    // Press 'left' to select the sea pen template breadcrumb.
    const homeBreadcrumb = allBreadcrumbs[0]!;
    homeBreadcrumb.focus();

    pressLeftKey(homeBreadcrumb);

    assertNotEquals('0', allBreadcrumbs[0]!.getAttribute('tabindex'));
    assertNotEquals('0', allBreadcrumbs[1]!.getAttribute('tabindex'));
    assertNotEquals('0', allBreadcrumbs[2]!.getAttribute('tabindex'));
    assertEquals('0', allBreadcrumbs[3]!.getAttribute('tabindex'));

    assertEquals(allBreadcrumbs[3], breadcrumbElement.$.selector.selectedItem);

    // Go to SeaPenCollection path to remove the sea pen template
    // breadcrumb.
    breadcrumbElement.path = Paths.SEA_PEN_COLLECTION;
    await waitAfterNextRender(breadcrumbElement);

    // There should now be 3 breadcrumbs.
    allBreadcrumbs = Array.from(breadcrumbElement.shadowRoot!.querySelectorAll(
                         '.selectable')) as HTMLElement[];
    assertEquals(3, allBreadcrumbs.length);

    // And tabindex for first breadcrumb should be 0.
    assertEquals('0', allBreadcrumbs[0]!.getAttribute('tabindex'));
    assertNotEquals('0', allBreadcrumbs[1]!.getAttribute('tabindex'));
    assertNotEquals('0', allBreadcrumbs[2]!.getAttribute('tabindex'));

    assertEquals(allBreadcrumbs[0], breadcrumbElement.$.selector.selectedItem);
  });
});

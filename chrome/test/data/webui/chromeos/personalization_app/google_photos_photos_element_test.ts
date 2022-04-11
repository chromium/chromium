// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {getNumberOfGridItemsPerRow, GooglePhotosPhoto, GooglePhotosPhotos, GooglePhotosPhotosSection, initializeGooglePhotosData, WallpaperGridItem, WallpaperLayout, WallpaperType} from 'chrome://personalization/trusted/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('GooglePhotosPhotosTest', function() {
  let googlePhotosPhotosElement: GooglePhotosPhotos|null;
  let personalizationStore: TestPersonalizationStore;
  let wallpaperProvider: TestWallpaperProvider;

  /**
   * Returns the match for |selector| in |googlePhotosPhotosElement|'s shadow
   * DOM.
   */
  function querySelector(selector: string): Element|null {
    return googlePhotosPhotosElement!.shadowRoot!.querySelector(selector);
  }

  /**
   * Returns all matches for |selector| in |googlePhotosPhotosElement|'s shadow
   * DOM.
   */
  function querySelectorAll(selector: string): Element[]|null {
    const matches =
        googlePhotosPhotosElement!.shadowRoot!.querySelectorAll(selector);
    return matches ? [...matches] : null;
  }

  /** Scrolls the specified |element| to the bottom. */
  async function scrollToBottom(element: HTMLElement) {
    element.scrollTop = element.scrollHeight;
    await waitAfterNextRender(googlePhotosPhotosElement!);
  }

  /**
   * Returns a list of |GooglePhotosPhotosSection|'s for the specified |photos|
   * and number of |photosPerRow|.
   */
  function toSections(photos: GooglePhotosPhoto[], photosPerRow: number):
      GooglePhotosPhotosSection[] {
    const sections: GooglePhotosPhotosSection[] = [];

    photos.forEach((photo, i) => {
      const title = toString(photo.date);

      // Find/create the appropriate |section| in which to insert |photo|.
      let section = sections[sections.length - 1];
      if (section?.title !== title) {
        section = {title, rows: []};
        sections.push(section);
      }

      // Find/create the appropriate |row| in which to insert |photo|.
      let row = section.rows[section.rows.length - 1];
      if ((row?.length ?? photosPerRow) === photosPerRow) {
        row = [];
        section.rows.push(row);
      }

      row!.push({...photo, index: i});
    });

    return sections;
  }

  /** Returns a |string| from the specified |value|. */
  function toString(value: String16): string {
    return value.data.map(c => String.fromCodePoint(c)).join('');
  }

  /** Returns a |String16| from the specified |value|. */
  function toString16(value: string): String16 {
    const data = [];
    for (let i = 0; i < value.length; ++i) {
      data[i] = value.charCodeAt(i);
    }
    return {data};
  }

  setup(() => {
    loadTimeData.overrideValues({'isGooglePhotosIntegrationEnabled': true});

    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    personalizationStore.setReducersEnabled(true);
    wallpaperProvider = mocks.wallpaperProvider;
  });

  teardown(async () => {
    await teardownElement(googlePhotosPhotosElement);
    googlePhotosPhotosElement = null;
  });

  test('displays photos', async () => {
    const photos: GooglePhotosPhoto[] = [
      {
        id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
        name: 'foo',
        date: toString16('Wednesday, February 16, 2022'),
        url: {url: 'foo.com'},
        location: 'home1'
      },
      {
        id: '0ec40478-9712-42e1-b5bf-3e75870ca042',
        name: 'bar',
        date: toString16('Friday, November 12, 2021'),
        url: {url: 'bar.com'},
        location: 'home2'
      },
      {
        id: '0a268a37-877a-4936-81d4-38cc84b0f596',
        name: 'baz',
        date: toString16('Friday, November 12, 2021'),
        url: {url: 'baz.com'},
        location: 'home3'
      }
    ];

    const sections =
        toSections(photos, /*photosPerRow=*/ getNumberOfGridItemsPerRow());

    // Set values returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosPhotos(photos);
    wallpaperProvider.setGooglePhotosCount(photos.length);

    // Initialize |googlePhotosPhotosElement|.
    googlePhotosPhotosElement =
        initElement(GooglePhotosPhotos, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosElement);

    // The |personalizationStore| should be empty, so no titles or |photos|
    // should be rendered initially.
    const titleSelector = '.title:not([hidden])';
    assertEquals(querySelectorAll(titleSelector)!.length, 0);
    const photoSelector =
        'wallpaper-grid-item:not([hidden]).photo:not([placeholder])';
    assertEquals(querySelectorAll(photoSelector)!.length, 0);

    // Initialize Google Photos data in the |personalizationStore|.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);
    await waitAfterNextRender(googlePhotosPhotosElement);

    // The wallpaper controller is expected to impose max resolution.
    photos.forEach(photo => photo.url.url += '=s512');

    // Verify that the number of rendered titles and |photos| is as expected.
    assertEquals(querySelectorAll(titleSelector)!.length, sections.length);
    assertEquals(querySelectorAll(photoSelector)!.length, photos.length);

    // Verify that the expected |sections| are rendered.
    let absoluteRowIndex = 0;
    sections.forEach(section => {
      section.rows.forEach((row, rowIndex) => {
        // Verify that the expected row is rendered.
        const rowEl = querySelector(
            `.row:not([hidden]):nth-of-type(${absoluteRowIndex + 1})`);
        assertNotEquals(rowEl, null);

        // Verify that the expected title is rendered.
        if (rowIndex === 0) {
          const titleEl = rowEl!.querySelector(titleSelector);
          assertNotEquals(titleEl, null);
          assertEquals(titleEl!.innerHTML, section.title);
        }

        // Verify that the expected |photos| are rendered.
        row.forEach((photo, photoIndex) => {
          const photoEl =
              rowEl!.querySelector(
                  `${photoSelector}:nth-of-type(${photoIndex + 1})`) as
                  WallpaperGridItem |
              null;
          assertNotEquals(photoEl, null);
          assertEquals(photoEl!.imageSrc, photo.url.url);
          assertEquals(photoEl!.primaryText, undefined);
          assertEquals(photoEl!.secondaryText, undefined);
        });

        ++absoluteRowIndex;
      });
    });
  });

  test('displays photo selected', async () => {
    const photo: GooglePhotosPhoto = {
      id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
      name: 'foo',
      date: {data: []},
      url: {url: 'foo.com'},
      location: 'home1'
    };

    const anotherPhoto: GooglePhotosPhoto = {
      id: '0ec40478-9712-42e1-b5bf-3e75870ca042',
      name: 'bar',
      date: {data: []},
      url: {url: 'bar.com'},
      location: 'home2'
    };

    // Set values returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosPhotos([photo, anotherPhoto]);
    wallpaperProvider.setGooglePhotosCount(2);

    // Initialize Google Photos data in the |personalizationStore|.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);

    // The wallpaper controller is expected to impose max resolution.
    photo.url.url += '=s512';
    anotherPhoto.url.url += '=s512';

    // Initialize |googlePhotosPhotosElement|.
    googlePhotosPhotosElement =
        initElement(GooglePhotosPhotos, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify that the expected photos are rendered.
    const photoSelector = 'wallpaper-grid-item:not([hidden]).photo';
    const photoEls = querySelectorAll(photoSelector) as WallpaperGridItem[];
    assertEquals(photoEls.length, 2);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, false);

    // Start a pending selection for |photo|.
    personalizationStore.data.wallpaper.pendingSelected = photo;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, true);
    assertEquals(photoEls[1]!.selected, false);

    // Complete the pending selection.
    personalizationStore.data.wallpaper.pendingSelected = null;
    personalizationStore.data.wallpaper.currentSelected = {
      url: photo.url,
      attribution: [],
      layout: WallpaperLayout.kCenter,
      type: WallpaperType.kGooglePhotos,
      key: photo.id
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, true);
    assertEquals(photoEls[1]!.selected, false);

    // Start a pending selection for |anotherPhoto|.
    personalizationStore.data.wallpaper.pendingSelected = anotherPhoto;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, true);

    // Complete the pending selection.
    personalizationStore.data.wallpaper.pendingSelected = null;
    personalizationStore.data.wallpaper.currentSelected = {
      url: anotherPhoto.url,
      attribution: [],
      layout: WallpaperLayout.kCenter,
      type: WallpaperType.kGooglePhotos,
      key: anotherPhoto.id
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, true);

    // Start a pending selection for a |FilePath| backed wallpaper.
    personalizationStore.data.wallpaper.pendingSelected = {path: '//foo'};
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, false);

    // Complete the pending selection.
    personalizationStore.data.wallpaper.pendingSelected = null;
    personalizationStore.data.wallpaper.currentSelected = {
      url: {url: 'foo://'},
      attribution: [],
      layout: WallpaperLayout.kCenter,
      type: WallpaperType.kCustomized,
      key: '//foo'
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, false);
  });

  test('displays placeholders until photos are present', async () => {
    // Prepare Google Photos data.
    const photosCount = 5;
    const photos: GooglePhotosPhoto[] =
        Array.from({length: photosCount}, (_, i) => ({
                                            id: `id-${i}`,
                                            name: `name-${i}`,
                                            date: {data: []},
                                            url: {url: `url-${i}`},
                                            location: `location-${i}`,
                                          }));

    // Initialize |googlePhotosPhotosElement|.
    googlePhotosPhotosElement =
        initElement(GooglePhotosPhotos, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Initially only placeholders should be present.
    const selector =
        '.row:not([hidden]) wallpaper-grid-item:not([hidden]).photo';
    const photoSelector = `${selector}:not([placeholder])`;
    const placeholderSelector = `${selector}[placeholder]`;
    assertEquals(querySelectorAll(photoSelector)!.length, 0);
    assertNotEquals(querySelectorAll(placeholderSelector)!.length, 0);

    // Clicking a placeholder should do nothing.
    const clickHandler = 'selectGooglePhotosPhoto';
    (querySelector(placeholderSelector) as HTMLElement).click();
    await new Promise<void>(resolve => setTimeout(resolve));
    assertEquals(wallpaperProvider.getCallCount(clickHandler), 0);

    // Provide Google Photos data.
    personalizationStore.data.wallpaper.googlePhotos.count = photosCount;
    personalizationStore.data.wallpaper.googlePhotos.photos = photos;
    personalizationStore.notifyObservers();

    // Only photos should be present.
    await waitAfterNextRender(googlePhotosPhotosElement);
    assertNotEquals(querySelectorAll(photoSelector)!.length, 0);
    assertEquals(querySelectorAll(placeholderSelector)!.length, 0);

    // Clicking a photo should do something.
    (querySelector(photoSelector) as HTMLElement).click();
    assertEquals(
        await wallpaperProvider.whenCalled(clickHandler), photos[0]!.id);
  });

  test('incrementally loads photos', async () => {
    // Set photos count returned by |wallpaperProvider|.
    const photosCount = 200;
    wallpaperProvider.setGooglePhotosCount(photosCount);

    // Set initial list of photos returned by |wallpaperProvider|.
    let nextPhotoId = 1;
    wallpaperProvider.setGooglePhotosPhotos(
        Array.from({length: photosCount / 2}).map(() => {
          return {
            id: `id-${nextPhotoId}`,
            name: `name-${nextPhotoId}`,
            date: {data: []},
            url: {url: `url-${nextPhotoId}`},
            location: `location-${nextPhotoId++}`,
          };
        }));

    // Set initial photos resume token returned  by |wallpaperProvider|. When
    // resume token is defined, it indicates additional photos exist.
    const resumeToken = 'resumeToken';
    wallpaperProvider.setGooglePhotosPhotosResumeToken(resumeToken);

    // Initialize Google Photos data in |personalizationStore|.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);
    assertDeepEquals(
        await wallpaperProvider.whenCalled('fetchGooglePhotosPhotos'),
        [/*itemId=*/ null, /*albumId=*/ null, /*resumeToken=*/ null]);

    // Reset |wallpaperProvider| expectations.
    wallpaperProvider.resetResolver('fetchGooglePhotosPhotos');

    // Set the next list of photos returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosPhotos(
        Array.from({length: photosCount / 2}).map(() => {
          return {
            id: `id-${nextPhotoId}`,
            name: `name-${nextPhotoId}`,
            date: {data: []},
            url: {url: `url-${nextPhotoId}`},
            location: `location-${nextPhotoId++}`,
          };
        }));

    // Set the next photos resume token returned by |wallpaperProvider|. When
    // resume token is undefined, it indicates no additional photos exist.
    wallpaperProvider.setGooglePhotosPhotosResumeToken(undefined);

    // Restrict the viewport so that |googlePhotosPhotosElement| will lazily
    // create photos instead of creating them all at once.
    const style = document.createElement('style');
    style.appendChild(document.createTextNode(`
      html,
      body {
        height: 100%;
        width: 100%;
      }
    `));
    document.head.appendChild(style);

    // Initialize |googlePhotosPhotosElement|.
    googlePhotosPhotosElement =
        initElement(GooglePhotosPhotos, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Scroll to the bottom of the grid.
    const gridScrollThreshold = googlePhotosPhotosElement.$.gridScrollThreshold;
    scrollToBottom(gridScrollThreshold);

    // Wait for and verify that the next batch of photos have been requested.
    assertDeepEquals(
        await wallpaperProvider.whenCalled('fetchGooglePhotosPhotos'),
        [/*itemId=*/ null, /*albumId=*/ null, /*resumeToken=*/ resumeToken]);
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Reset |wallpaperProvider| expectations.
    wallpaperProvider.resetResolver('fetchGooglePhotosPhotos');

    // Scroll to the bottom of the grid.
    scrollToBottom(gridScrollThreshold);

    // Verify that no next batch of photos has been requested.
    assertEquals(wallpaperProvider.getCallCount('fetchGooglePhotosPhotos'), 0);
  });

  test('selects photo', async () => {
    const photo: GooglePhotosPhoto = {
      id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
      name: 'foo',
      date: {data: []},
      url: {url: 'foo.com'},
      location: 'home'
    };

    // Set values returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosPhotos([photo]);
    wallpaperProvider.setGooglePhotosCount(1);

    // Initialize Google Photos data in the |personalizationStore|.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);

    // The wallpaper controller is expected to impose max resolution.
    photo.url.url += '=s512';

    // Initialize |googlePhotosPhotosElement|.
    googlePhotosPhotosElement =
        initElement(GooglePhotosPhotos, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosElement);

    // Verify that the expected |photo| is rendered.
    const photoSelector = 'wallpaper-grid-item:not([hidden]).photo';
    const photoEls = querySelectorAll(photoSelector) as WallpaperGridItem[];
    assertEquals(photoEls.length, 1);
    assertEquals(photoEls[0]!.imageSrc, photo.url.url);
    assertEquals(photoEls[0]!.primaryText, undefined);
    assertEquals(photoEls[0]!.secondaryText, undefined);

    // Select |photo| and verify selection started.
    photoEls[0]!.click();
    assertEquals(personalizationStore.data.wallpaper.loading.setImage, 1);
    assertEquals(personalizationStore.data.wallpaper.loading.selected, true);
    assertDeepEquals(
        personalizationStore.data.wallpaper.pendingSelected,
        {...photo, index: 0});

    // Wait for and verify hard-coded selection failure.
    const methodName = 'selectGooglePhotosPhoto';
    assertEquals(await wallpaperProvider.whenCalled(methodName), photo.id);
    await waitAfterNextRender(googlePhotosPhotosElement);
    assertEquals(personalizationStore.data.wallpaper.loading.setImage, 0);
    assertEquals(personalizationStore.data.wallpaper.loading.selected, false);
    assertEquals(personalizationStore.data.wallpaper.pendingSelected, null);
  });
});

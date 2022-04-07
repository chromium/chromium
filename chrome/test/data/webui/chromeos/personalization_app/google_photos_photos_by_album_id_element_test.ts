// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {fetchGooglePhotosAlbum, GooglePhotosAlbum, GooglePhotosEnablementState, GooglePhotosPhoto, GooglePhotosPhotosByAlbumId, initializeGooglePhotosData, WallpaperGridItem, WallpaperLayout, WallpaperType} from 'chrome://personalization/trusted/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertDeepEquals, assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('GooglePhotosPhotosByAlbumIdTest', function() {
  let googlePhotosPhotosByAlbumIdElement: GooglePhotosPhotosByAlbumId|null;
  let personalizationStore: TestPersonalizationStore;
  let wallpaperProvider: TestWallpaperProvider;

  /**
   * Returns the match for |selector| in |googlePhotosPhotosByAlbumIdElement|'s
   * shadow DOM.
   */
  function querySelector(selector: string): Element|null {
    return googlePhotosPhotosByAlbumIdElement!.shadowRoot!.querySelector(
        selector);
  }

  /**
   * Returns all matches for |selector| in
   * |googlePhotosPhotosByAlbumIdElement|'s shadow DOM.
   */
  function querySelectorAll(selector: string): Element[]|null {
    const matches =
        googlePhotosPhotosByAlbumIdElement!.shadowRoot!.querySelectorAll(
            selector);
    return matches ? [...matches] : null;
  }

  /** Scrolls the specified |element| to the bottom. */
  async function scrollToBottom(element: HTMLElement) {
    element.scrollTop = element.scrollHeight;
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement!);
  }

  setup(() => {
    loadTimeData.overrideValues({'isGooglePhotosIntegrationEnabled': true});

    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    wallpaperProvider = mocks.wallpaperProvider;
  });

  teardown(async () => {
    await teardownElement(googlePhotosPhotosByAlbumIdElement);
    googlePhotosPhotosByAlbumIdElement = null;
  });

  test('displays photos for the selected album id', async () => {
    const album: GooglePhotosAlbum = {
      id: '1',
      title: 'foo',
      photoCount: 2,
      preview: {url: 'foo.com'},
    };

    const otherAlbum: GooglePhotosAlbum = {
      id: '2',
      title: 'bar',
      photoCount: 1,
      preview: {url: 'bar.com'},
    };

    const photosByAlbumId: Record<string, GooglePhotosPhoto[]> = {
      [album.id]: [
        {
          id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
          name: 'foo',
          date: {data: []},
          url: {url: 'foo.com'},
          location: 'home1'
        },
        {
          id: '0ec40478-9712-42e1-b5bf-3e75870ca042',
          name: 'bar',
          date: {data: []},
          url: {url: 'bar.com'},
          location: 'home2'
        },
      ],
      [otherAlbum.id]: [
        {
          id: '0a268a37-877a-4936-81d4-38cc84b0f596',
          name: 'baz',
          date: {data: []},
          url: {url: 'baz.com'},
          location: 'home3'
        },
      ],
    };

    // Allow access to Google Photos.
    personalizationStore.data.wallpaper.googlePhotos.enabled =
        GooglePhotosEnablementState.kEnabled;

    // Initialize |googlePhotosPhotosByAlbumIdElement|.
    googlePhotosPhotosByAlbumIdElement =
        initElement(GooglePhotosPhotosByAlbumId, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Initially no album id selected. Photos should be absent.
    const photoSelector =
        'wallpaper-grid-item:not([hidden]):not([placeholder]).photo';
    assertEquals(querySelectorAll(photoSelector)!.length, 0);

    // Select an album id. Photos should be absent since albums have not loaded.
    googlePhotosPhotosByAlbumIdElement.setAttribute('album-id', album.id);
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(querySelectorAll(photoSelector)!.length, 0);

    // Load albums. Photos should be absent since they are not loaded.
    personalizationStore.data.wallpaper.googlePhotos.albums =
        [album, otherAlbum];
    personalizationStore.notifyObservers();

    // Expect a request to load photos for the selected album id.
    assertDeepEquals(
        await wallpaperProvider.whenCalled('fetchGooglePhotosPhotos'),
        [/*itemId=*/ null, /*albumId=*/ album.id, /*resumeToken=*/ null]);

    // Start loading photos for the selected album id. Photos should still be
    // absent since they are still not loaded.
    personalizationStore.data.wallpaper.loading.googlePhotos.photosByAlbumId = {
      ...personalizationStore.data.wallpaper.loading.googlePhotos
          .photosByAlbumId,
      [album.id]: true,
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(querySelectorAll(photoSelector)!.length, 0);

    // Load photos for an album id other than that which is selected. Photos
    // should still be absent since they are still not loaded for the selected
    // album id.
    personalizationStore.data.wallpaper.googlePhotos.photosByAlbumId = {
      ...personalizationStore.data.wallpaper.googlePhotos.photosByAlbumId,
      [otherAlbum.id]: photosByAlbumId[otherAlbum.id],
    };
    personalizationStore.data.wallpaper.loading.googlePhotos.photosByAlbumId = {
      ...personalizationStore.data.wallpaper.loading.googlePhotos
          .photosByAlbumId,
      [otherAlbum.id]: false,
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(querySelectorAll(photoSelector)!.length, 0);

    // Finish loading photos for the selected album id. Photos should now be
    // present since they are finished loading for the selected album id.
    personalizationStore.data.wallpaper.googlePhotos.photosByAlbumId = {
      ...personalizationStore.data.wallpaper.googlePhotos.photosByAlbumId,
      [album.id]: photosByAlbumId[album.id],
    };
    personalizationStore.data.wallpaper.loading.googlePhotos.photosByAlbumId = {
      ...personalizationStore.data.wallpaper.loading.googlePhotos
          .photosByAlbumId,
      [album.id]: false,
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    const photosEls = querySelectorAll(photoSelector) as WallpaperGridItem[];
    assertEquals(photosEls.length, photosByAlbumId[album.id]?.length);
    photosEls.forEach((photoEl, i) => {
      assertEquals(photoEl.imageSrc, photosByAlbumId[album.id]![i]!.url.url);
    });

    // Select the other album id for which data is already loaded. Photos should
    // immediately update since they are already loaded for the selected album
    // id.
    googlePhotosPhotosByAlbumIdElement.setAttribute('album-id', otherAlbum.id);
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(
        querySelectorAll(photoSelector)!.length,
        photosByAlbumId[otherAlbum.id]?.length);

    // Un-select the album id. Photos should be absent since no album id is
    // selected.
    googlePhotosPhotosByAlbumIdElement.removeAttribute('album-id');
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(querySelectorAll(photoSelector)!.length, 0);
  });

  test('displays photo selected', async () => {
    personalizationStore.setReducersEnabled(true);

    const album: GooglePhotosAlbum =
        {id: '1', title: '', photoCount: 2, preview: {url: ''}};

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
    wallpaperProvider.setGooglePhotosAlbums([album]);
    wallpaperProvider.setGooglePhotosCount(2);
    wallpaperProvider.setGooglePhotosPhotos([photo, anotherPhoto]);
    wallpaperProvider.setGooglePhotosPhotosByAlbumId(
        album.id, [photo, anotherPhoto]);

    // Initialize Google Photos data in the |personalizationStore|.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);

    // The wallpaper controller is expected to impose max resolution.
    album.preview.url += '=s512';
    photo.url.url += '=s512';
    anotherPhoto.url.url += '=s512';

    // Initialize |googlePhotosPhotosByAlbumIdElement|.
    googlePhotosPhotosByAlbumIdElement =
        initElement(GooglePhotosPhotosByAlbumId, {hidden: false});
    googlePhotosPhotosByAlbumIdElement.setAttribute('album-id', album.id);
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

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
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

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
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, true);
    assertEquals(photoEls[1]!.selected, false);

    // Start a pending selection for |anotherPhoto|.
    personalizationStore.data.wallpaper.pendingSelected = anotherPhoto;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

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
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, true);

    // Start a pending selection for a |FilePath| backed wallpaper.
    personalizationStore.data.wallpaper.pendingSelected = {path: '//foo'};
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

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
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, false);
  });

  test('displays placeholders until photos are present', async () => {
    // Prepare Google Photos data.
    const photosCount = 5;
    const album: GooglePhotosAlbum =
        {id: '1', title: '', photoCount: photosCount, preview: {url: ''}};
    const photos: GooglePhotosPhoto[] =
        Array.from({length: photosCount}, (_, i) => ({
                                            id: `id-${i}`,
                                            name: `name-${i}`,
                                            date: {data: []},
                                            url: {url: `url-${i}`},
                                            location: `location-${i}`,
                                          }));

    // Initialize |googlePhotosPhotosByAlbumIdElement|.
    googlePhotosPhotosByAlbumIdElement =
        initElement(GooglePhotosPhotosByAlbumId, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Initially no album id selected. Photos and placeholders should be absent.
    const selector = 'wallpaper-grid-item:not([hidden]).photo';
    const photoSelector = `${selector}:not([placeholder])`;
    const placeholderSelector = `${selector}[placeholder]`;
    assertEquals(querySelectorAll(photoSelector)!.length, 0);
    assertEquals(querySelectorAll(placeholderSelector)!.length, 0);

    // Select album id. Only placeholders should be present.
    googlePhotosPhotosByAlbumIdElement.setAttribute('album-id', album.id);
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(querySelectorAll(photoSelector)!.length, 0);
    assertNotEquals(querySelectorAll(placeholderSelector)!.length, 0);

    // Clicking a placeholder should do nothing.
    const clickHandler = 'selectGooglePhotosPhoto';
    (querySelector(placeholderSelector) as HTMLElement).click();
    await new Promise<void>(resolve => setTimeout(resolve));
    assertEquals(wallpaperProvider.getCallCount(clickHandler), 0);

    // Provide Google Photos data.
    personalizationStore.data.wallpaper.googlePhotos.count = photosCount;
    personalizationStore.data.wallpaper.googlePhotos.albums = [album];
    personalizationStore.data.wallpaper.googlePhotos.photos = photos;
    personalizationStore.data.wallpaper.googlePhotos
        .photosByAlbumId = {[album.id]: photos};
    personalizationStore.notifyObservers();

    // Only photos should be present.
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertNotEquals(querySelectorAll(photoSelector)!.length, 0);
    assertEquals(querySelectorAll(placeholderSelector)!.length, 0);

    // Clicking a photo should do something.
    (querySelector(photoSelector) as HTMLElement).click();
    assertEquals(
        await wallpaperProvider.whenCalled(clickHandler), photos[0]!.id);
  });

  test('incrementally loads photos', async () => {
    personalizationStore.setReducersEnabled(true);

    const photosCount = 200;
    const album: GooglePhotosAlbum =
        {id: '1', title: '', photoCount: photosCount, preview: {url: ''}};

    // Set photos count returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosCount(photosCount);

    // Set albums returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosAlbums([album]);

    // Set initial list of photos returned by |wallpaperProvider|.
    let nextPhotoId = 1;
    wallpaperProvider.setGooglePhotosPhotosByAlbumId(
        album.id, Array.from({length: photosCount / 2}).map(() => {
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
    wallpaperProvider.setGooglePhotosPhotosByAlbumIdResumeToken(
        album.id, resumeToken);

    // Initialize Google Photos data in |personalizationStore|.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);
    assertDeepEquals(
        await wallpaperProvider.whenCalled('fetchGooglePhotosPhotos'),
        [/*itemId=*/ null, /*albumId=*/ null, /*resumeToken=*/ null]);

    // Reset |wallpaperProvider| expectations.
    wallpaperProvider.resetResolver('fetchGooglePhotosPhotos');

    // Initialize Google Photos album in |personalizationStore|.
    await fetchGooglePhotosAlbum(
        wallpaperProvider, personalizationStore, album.id);
    assertDeepEquals(
        await wallpaperProvider.whenCalled('fetchGooglePhotosPhotos'),
        [/*itemId=*/ null, album.id, /*resumeToken=*/ null]);

    // Reset |wallpaperProvider| expectations.
    wallpaperProvider.resetResolver('fetchGooglePhotosPhotos');

    // Set the next list of photos returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosPhotosByAlbumId(
        album.id, Array.from({length: photosCount / 2}).map(() => {
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
    wallpaperProvider.setGooglePhotosPhotosByAlbumIdResumeToken(
        album.id, undefined);

    // Restrict the viewport so that |googlePhotosPhotosByAlbumIdElement| will
    // lazily create photos instead of creating them all at once.
    const style = document.createElement('style');
    style.appendChild(document.createTextNode(`
      html,
      body {
        height: 100%;
        width: 100%;
      }
    `));
    document.head.appendChild(style);

    // Initialize |googlePhotosPhotosByAlbumIdElement|.
    googlePhotosPhotosByAlbumIdElement =
        initElement(GooglePhotosPhotosByAlbumId, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Select |album|.
    googlePhotosPhotosByAlbumIdElement.setAttribute('album-id', album.id);
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Scroll to the bottom of the grid.
    const gridScrollThreshold =
        googlePhotosPhotosByAlbumIdElement.$.gridScrollThreshold;
    scrollToBottom(gridScrollThreshold);

    // Wait for and verify that the next batch of photos have been requested.
    assertDeepEquals(
        await wallpaperProvider.whenCalled('fetchGooglePhotosPhotos'),
        [/*itemId=*/ null, album.id, /*resumeToken=*/ resumeToken]);
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Reset |wallpaperProvider| expectations.
    wallpaperProvider.resetResolver('fetchGooglePhotosPhotos');

    // Scroll to the bottom of the grid.
    scrollToBottom(gridScrollThreshold);

    // Verify that no next batch of photos has been requested.
    assertEquals(wallpaperProvider.getCallCount('fetchGooglePhotosPhotos'), 0);
  });

  test('selects photo', async () => {
    personalizationStore.setReducersEnabled(true);

    const album: GooglePhotosAlbum = {
      id: '1',
      title: 'foo',
      photoCount: 1,
      preview: {url: 'foo.com'},
    };

    const photo: GooglePhotosPhoto = {
      id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
      name: 'foo',
      date: {data: []},
      url: {url: 'foo.com'},
      location: 'home'
    };

    // Initialize Google Photos data in the |personalizationStore|.
    personalizationStore.data.wallpaper.googlePhotos.albums = [album];
    personalizationStore.data.wallpaper.googlePhotos
        .photosByAlbumId = {[album.id]: [photo]};

    // Initialize |googlePhotosPhotosByAlbumIdElement|.
    googlePhotosPhotosByAlbumIdElement =
        initElement(GooglePhotosPhotosByAlbumId, {hidden: false});
    googlePhotosPhotosByAlbumIdElement.setAttribute('album-id', album.id);
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

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
    assertEquals(personalizationStore.data.wallpaper.pendingSelected, photo);

    // Wait for and verify hard-coded selection failure.
    const methodName = 'selectGooglePhotosPhoto';
    assertEquals(await wallpaperProvider.whenCalled(methodName), photo.id);
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(personalizationStore.data.wallpaper.loading.setImage, 0);
    assertEquals(personalizationStore.data.wallpaper.loading.selected, false);
    assertEquals(personalizationStore.data.wallpaper.pendingSelected, null);
  });
});

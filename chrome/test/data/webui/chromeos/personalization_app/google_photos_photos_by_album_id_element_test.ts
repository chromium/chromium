// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {fetchGooglePhotosAlbum, fetchGooglePhotosAlbums, fetchGooglePhotosEnabled, fetchGooglePhotosPhotos, GooglePhotosAlbum, GooglePhotosEnablementState, GooglePhotosPhoto, GooglePhotosPhotosByAlbumIdElement, PersonalizationActionName, SetErrorAction, WallpaperGridItemElement, WallpaperLayout, WallpaperType} from 'chrome://personalization/js/personalization_app.js';
import {assertDeepEquals, assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, createSvgDataUrl, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('GooglePhotosPhotosByAlbumIdElementTest', function() {
  let googlePhotosPhotosByAlbumIdElement: GooglePhotosPhotosByAlbumIdElement|
      null;
  let personalizationStore: TestPersonalizationStore;
  let wallpaperProvider: TestWallpaperProvider;

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

  /** Scrolls the window to the bottom. */
  async function scrollToBottom() {
    window.scroll({top: document.body.scrollHeight, behavior: 'smooth'});
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement!);
  }

  setup(() => {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    wallpaperProvider = mocks.wallpaperProvider;
  });

  teardown(async () => {
    await teardownElement(googlePhotosPhotosByAlbumIdElement);
    googlePhotosPhotosByAlbumIdElement = null;
  });

  [true, false].forEach(
      (dismissFromUser: boolean) =>
          test('displays error when selected album fails to load', async () => {
            personalizationStore.setReducersEnabled(true);

            const album: GooglePhotosAlbum = {
              id: '1',
              title: 'foo',
              photoCount: 1,
              preview: {url: 'foo.com'},
              timestamp: {internalValue: BigInt('1')},
              isShared: false,
            };

            // Set values returned by |wallpaperProvider|.
            wallpaperProvider.setGooglePhotosAlbums([album]);
            wallpaperProvider.setGooglePhotosPhotosByAlbumId(album.id, null);

            // Initialize Google Photos data in the |personalizationStore|.
            await fetchGooglePhotosEnabled(
                wallpaperProvider, personalizationStore);
            await fetchGooglePhotosAlbums(
                wallpaperProvider, personalizationStore);

            // Initialize |googlePhotosPhotosByAlbumIdElement|.
            googlePhotosPhotosByAlbumIdElement = initElement(
                GooglePhotosPhotosByAlbumIdElement, {hidden: false});
            await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

            // Select |album| and expect an |error|.
            personalizationStore.expectAction(
                PersonalizationActionName.SET_ERROR);
            googlePhotosPhotosByAlbumIdElement.setAttribute(
                'album-id', album.id);
            const {error} =
                await personalizationStore.waitForAction(
                    PersonalizationActionName.SET_ERROR) as SetErrorAction;

            // Verify |error| expectations.
            assertEquals(
                error.message,
                'Couldn’t load images. Check your network connection or try loading the images again.');
            assertEquals(error.dismiss?.message, 'Try again');
            assertNotEquals(error.dismiss?.callback, undefined);

            wallpaperProvider.reset();

            // Simulate dismissal of |error| conditionally |fromUser| and verify
            // expected interactions with wallpaper provider.
            error.dismiss?.callback?.(/*fromUser=*/ dismissFromUser);
            await new Promise<void>(resolve => setTimeout(resolve));
            assertEquals(
                wallpaperProvider.getCallCount('fetchGooglePhotosPhotos'),
                dismissFromUser ? 1 : 0);

            wallpaperProvider.reset();

            // Simulate hiding |googlePhotosPhotosByAlbumIdElement| and verify
            // the |error| is dismissed though not |fromUser|.
            const dismissCallbackPromise = new Promise<boolean>(resolve => {
              personalizationStore.data.error!.dismiss!.callback = resolve;
            });
            googlePhotosPhotosByAlbumIdElement.hidden = true;
            assertEquals(await dismissCallbackPromise, /*fromUser=*/ false);
            await new Promise<void>(resolve => setTimeout(resolve));
            assertEquals(
                wallpaperProvider.getCallCount('fetchGooglePhotosPhotos'), 0);
          }));

  test('displays photos for the selected album id', async () => {
    const album: GooglePhotosAlbum = {
      id: '1',
      title: 'foo',
      photoCount: 2,
      // Use svg data urls so that img on-load event fires and removes the
      // placeholder attribute.
      preview: {url: createSvgDataUrl('svg-1')},
      timestamp: {internalValue: BigInt('1')},
      isShared: false,
    };

    const otherAlbum: GooglePhotosAlbum = {
      id: '2',
      title: 'bar',
      photoCount: 1,
      preview: {url: createSvgDataUrl('svg-2')},
      timestamp: {internalValue: BigInt('2')},
      isShared: false,
    };

    const photosByAlbumId: Record<string, GooglePhotosPhoto[]> = {
      [album.id]: [
        {
          id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
          dedupKey: '2d0d1595-14af-4471-b2db-b9c8eae3a491',
          name: 'foo',
          date: {data: []},
          url: {url: createSvgDataUrl('svg-3')},
          location: 'home1',
        },
        {
          id: '0ec40478-9712-42e1-b5bf-3e75870ca042',
          dedupKey: '2cb1b955-0b7e-4f59-b9d0-802227aeeb28',
          name: 'bar',
          date: {data: []},
          url: {url: createSvgDataUrl('svg-4')},
          location: 'home2',
        },
      ],
      [otherAlbum.id]: [
        {
          id: '0a268a37-877a-4936-81d4-38cc84b0f596',
          dedupKey: 'd99eedfa-43e5-4bca-8882-b881222b8db9',
          name: 'baz',
          date: {data: []},
          url: {url: createSvgDataUrl('svg-5')},
          location: 'home3',
        },
      ],
    };

    // Allow access to Google Photos.
    personalizationStore.data.wallpaper.googlePhotos.enabled =
        GooglePhotosEnablementState.kEnabled;

    // Initialize |googlePhotosPhotosByAlbumIdElement|.
    googlePhotosPhotosByAlbumIdElement =
        initElement(GooglePhotosPhotosByAlbumIdElement, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    const photoSelector =
        'wallpaper-grid-item:not([hidden]):not([placeholder]).photo';
    assertEquals(
        querySelectorAll(photoSelector)!.length, 0,
        'Initially no album id selected so photos should be absent');

    googlePhotosPhotosByAlbumIdElement.setAttribute('album-id', album.id);
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(
        querySelectorAll(photoSelector)!.length, 0,
        'photos should be absent since albums have not loaded');

    // Load albums. Photos should be absent since they are not loaded.
    personalizationStore.data.wallpaper.googlePhotos.albums =
        [album, otherAlbum];
    personalizationStore.notifyObservers();

    // Expect a request to load photos for the selected album id.
    assertDeepEquals(
        await wallpaperProvider.whenCalled('fetchGooglePhotosPhotos'),
        [/*itemId=*/ null, /*albumId=*/ album.id, /*resumeToken=*/ null]);

    // Start loading photos for the selected album id.
    personalizationStore.data.wallpaper.loading.googlePhotos.photosByAlbumId = {
      ...personalizationStore.data.wallpaper.loading.googlePhotos
          .photosByAlbumId,
      [album.id]: true,
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(
        querySelectorAll(photoSelector)!.length, 0,
        'photos should still be absent since they are still not loaded');

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
    assertEquals(
        querySelectorAll(photoSelector)!.length, 0,
        'photos still not loaded for selected album id');

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
    const photosEls =
        querySelectorAll(photoSelector) as WallpaperGridItemElement[];
    assertEquals(
        photosEls.length, photosByAlbumId[album.id]?.length,
        'correct number of photo elements for album');
    photosEls.forEach((photoEl, i) => {
      assertEquals(
          photoEl.src, photosByAlbumId[album.id]![i]!.url,
          `correct url for album.id ${album.id} index ${i}`);
    });

    // Select the other album id for which data is already loaded. Photos should
    // immediately update since they are already loaded for the selected album
    // id.
    googlePhotosPhotosByAlbumIdElement.setAttribute('album-id', otherAlbum.id);
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(
        querySelectorAll(photoSelector)!.length,
        photosByAlbumId[otherAlbum.id]?.length,
        'correct number of photos visible for other album id');

    // Un-select the album id.
    googlePhotosPhotosByAlbumIdElement.removeAttribute('album-id');
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(
        querySelectorAll(photoSelector)!.length, 0,
        'photos should be absent since no album id selected');
  });

  test('displays photo selected', async () => {
    personalizationStore.setReducersEnabled(true);

    const album: GooglePhotosAlbum = {
      id: '1',
      title: '',
      photoCount: 2,
      preview: {url: ''},
      timestamp: {internalValue: BigInt('1')},
      isShared: false,
    };

    const photo: GooglePhotosPhoto = {
      id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
      dedupKey: '2d0d1595-14af-4471-b2db-b9c8eae3a491',
      name: 'foo',
      date: {data: []},
      url: {url: 'foo.com'},
      location: 'home1',
    };

    const anotherPhoto: GooglePhotosPhoto = {
      id: '0ec40478-9712-42e1-b5bf-3e75870ca042',
      dedupKey: '2cb1b955-0b7e-4f59-b9d0-802227aeeb28',
      name: 'bar',
      date: {data: []},
      url: {url: 'bar.com'},
      location: 'home2',
    };

    const yetAnotherPhoto: GooglePhotosPhoto = {
      id: '0a268a37-877a-4936-81d4-38cc84b0f596',
      dedupKey: photo.dedupKey,
      name: 'baz',
      date: {data: []},
      url: {url: 'baz.com'},
      location: 'home3',
    };

    // Set values returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosAlbums([album]);
    wallpaperProvider.setGooglePhotosPhotos(
        [photo, anotherPhoto, yetAnotherPhoto]);
    wallpaperProvider.setGooglePhotosPhotosByAlbumId(
        album.id, [photo, anotherPhoto]);

    // Initialize Google Photos data in the |personalizationStore|.
    await fetchGooglePhotosEnabled(wallpaperProvider, personalizationStore);
    await fetchGooglePhotosAlbums(wallpaperProvider, personalizationStore);
    await fetchGooglePhotosPhotos(wallpaperProvider, personalizationStore);

    // The wallpaper controller is expected to impose max resolution.
    album.preview.url += '=s512';
    photo.url.url += '=s512';
    anotherPhoto.url.url += '=s512';
    yetAnotherPhoto.url.url += '=s512';

    // Initialize |googlePhotosPhotosByAlbumIdElement|.
    googlePhotosPhotosByAlbumIdElement =
        initElement(GooglePhotosPhotosByAlbumIdElement, {hidden: false});
    googlePhotosPhotosByAlbumIdElement.setAttribute('album-id', album.id);
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Verify that the expected photos are rendered.
    const photoSelector = 'wallpaper-grid-item:not([hidden]).photo';
    const photoEls =
        querySelectorAll(photoSelector) as WallpaperGridItemElement[];
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
      descriptionContent: '',
      descriptionTitle: '',
      key: photo.id,
      layout: WallpaperLayout.kCenter,
      type: WallpaperType.kOnceGooglePhotos,
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
      descriptionContent: '',
      descriptionTitle: '',
      key: anotherPhoto.id,
      layout: WallpaperLayout.kCenter,
      type: WallpaperType.kOnceGooglePhotos,
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, true);

    // Start a pending selection for |yetAnotherPhoto|.
    personalizationStore.data.wallpaper.pendingSelected = yetAnotherPhoto;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, true);
    assertEquals(photoEls[1]!.selected, false);

    // Complete the pending selection.
    personalizationStore.data.wallpaper.pendingSelected = null;
    personalizationStore.data.wallpaper.currentSelected = {
      descriptionContent: '',
      descriptionTitle: '',
      key: yetAnotherPhoto.dedupKey!,
      layout: WallpaperLayout.kCenter,
      type: WallpaperType.kOnceGooglePhotos,
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, true);
    assertEquals(photoEls[1]!.selected, false);

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
      descriptionContent: '',
      descriptionTitle: '',
      key: '//foo',
      layout: WallpaperLayout.kCenter,
      type: WallpaperType.kCustomized,
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
    const album: GooglePhotosAlbum = {
      id: '1',
      title: '',
      photoCount: photosCount,
      preview: {url: ''},
      timestamp: {internalValue: BigInt('1')},
      isShared: false,
    };
    const photos: GooglePhotosPhoto[] = Array.from(
        {length: photosCount}, (_, i) => ({
                                 id: `id-${i}`,
                                 dedupKey: `dedupKey-${i}`,
                                 name: `name-${i}`,
                                 date: {data: []},
                                 url: {url: createSvgDataUrl(`svg-${i}`)},
                                 location: `location-${i}`,
                               }));

    // Initialize |googlePhotosPhotosByAlbumIdElement|.
    googlePhotosPhotosByAlbumIdElement =
        initElement(GooglePhotosPhotosByAlbumIdElement, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Initially no album id selected. Photos and placeholders should be absent.
    const selector = 'wallpaper-grid-item:not([hidden]).photo';
    const photoSelector = `${selector}:not([placeholder])`;
    const placeholderSelector = `${selector}[placeholder]`;
    const photoListSelector = 'iron-list:not([hidden])#grid';
    assertEquals(querySelectorAll(photoSelector)!.length, 0);
    let placeholderEls = querySelectorAll(placeholderSelector);
    assertEquals(placeholderEls!.length, 0);
    let photoListEl = querySelectorAll(photoListSelector);
    assertEquals(photoListEl!.length, 1);
    assertEquals(
        photoListEl![0]!.getAttribute('aria-setsize'),
        placeholderEls!.length.toString());

    // Select album id. Only placeholders should be present.
    googlePhotosPhotosByAlbumIdElement.setAttribute('album-id', album.id);
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(querySelectorAll(photoSelector)!.length, 0);
    placeholderEls = querySelectorAll(placeholderSelector);
    assertNotEquals(placeholderEls!.length, 0);
    photoListEl = querySelectorAll(photoListSelector);
    assertEquals(photoListEl!.length, 1);
    assertEquals(
        photoListEl![0]!.getAttribute('aria-setsize'),
        placeholderEls!.length.toString());

    // Placeholders should be aria-labeled.
    placeholderEls!.forEach(placeholderEl => {
      assertEquals(placeholderEl.getAttribute('aria-label'), 'Loading');
    });

    // Clicking a placeholder should do nothing.
    const clickHandler = 'selectGooglePhotosPhoto';
    (placeholderEls![0] as HTMLElement).click();
    await new Promise<void>(resolve => setTimeout(resolve));
    assertEquals(wallpaperProvider.getCallCount(clickHandler), 0);
    assertEquals(placeholderEls![0]!.getAttribute('aria-disabled'), 'true');

    // Provide Google Photos data.
    personalizationStore.data.wallpaper.googlePhotos.albums = [album];
    personalizationStore.data.wallpaper.googlePhotos.photos = photos;
    personalizationStore.data.wallpaper.googlePhotos
        .photosByAlbumId = {[album.id]: photos};
    personalizationStore.notifyObservers();

    // Only photos should be present.
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    const photoEls = querySelectorAll(photoSelector);
    assertNotEquals(photoEls!.length, 0);
    assertEquals(querySelectorAll(placeholderSelector)!.length, 0);

    // The photo list's aria-setsize should be consistent with the number of
    // photos.
    photoListEl = querySelectorAll(photoListSelector);
    assertEquals(photoListEl!.length, 1);
    assertEquals(
        photoListEl![0]!.getAttribute('aria-setsize'),
        photos.length.toString());

    // Photos should be aria-labeled.
    photoEls!.forEach((photoEl, i) => {
      assertEquals(photoEl.getAttribute('aria-label'), photos[i]!.name);
      assertEquals(photoEl.getAttribute('aria-posinset'), (i + 1).toString());
    });

    // Clicking a photo should do something.
    (photoEls![0] as HTMLElement).click();
    assertEquals(
        await wallpaperProvider.whenCalled(clickHandler), photos[0]!.id);
    assertEquals(photoEls![0]!.getAttribute('aria-disabled'), 'false');
  });

  test('incrementally loads photos', async () => {
    personalizationStore.setReducersEnabled(true);

    const photosCount = 200;
    const album: GooglePhotosAlbum = {
      id: '1',
      title: '',
      photoCount: photosCount,
      preview: {url: ''},
      timestamp: {internalValue: BigInt('1')},
      isShared: false,
    };

    // Set albums returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosAlbums([album]);

    // Set initial list of photos returned by |wallpaperProvider|.
    let nextPhotoId = 1;
    wallpaperProvider.setGooglePhotosPhotosByAlbumId(
        album.id, Array.from({length: photosCount / 2}).map(() => {
          return {
            id: `id-${nextPhotoId}`,
            dedupKey: `dedupKey-${nextPhotoId}`,
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
    await fetchGooglePhotosEnabled(wallpaperProvider, personalizationStore);
    await fetchGooglePhotosAlbums(wallpaperProvider, personalizationStore);
    await fetchGooglePhotosPhotos(wallpaperProvider, personalizationStore);
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
            dedupKey: `dedupKey-${nextPhotoId}`,
            name: `name-${nextPhotoId}`,
            date: {data: []},
            url: {url: `url-${nextPhotoId}`},
            location: `location-${nextPhotoId++}`,
          };
        }));

    // Set the next photos resume token returned by |wallpaperProvider|. When
    // resume token is null, it indicates no additional photos exist.
    wallpaperProvider.setGooglePhotosPhotosByAlbumIdResumeToken(album.id, null);

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
        initElement(GooglePhotosPhotosByAlbumIdElement, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Select |album|.
    googlePhotosPhotosByAlbumIdElement.setAttribute('album-id', album.id);
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Scroll to the bottom of the grid.
    scrollToBottom();

    // Wait for and verify that the next batch of photos have been requested.
    assertDeepEquals(
        await wallpaperProvider.whenCalled('fetchGooglePhotosPhotos'),
        [/*itemId=*/ null, album.id, /*resumeToken=*/ resumeToken]);
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Reset |wallpaperProvider| expectations.
    wallpaperProvider.resetResolver('fetchGooglePhotosPhotos');

    // Scroll to the bottom of the grid.
    scrollToBottom();

    // Verify that no next batch of photos has been requested.
    assertEquals(wallpaperProvider.getCallCount('fetchGooglePhotosPhotos'), 0);
  });

  test('reattempts failed photos load on show', async () => {
    const album: GooglePhotosAlbum = {
      id: '1',
      title: '',
      photoCount: 0,
      isShared: false,
      preview: {url: ''},
      timestamp: {internalValue: BigInt(0)},
    };

    // Initialize Google Photos data in the |personalizationStore| such as would
    // occur if photos for an album were previously fetched but failed to load.
    personalizationStore.data.wallpaper.loading.googlePhotos
        .photosByAlbumId[album.id] = false;
    personalizationStore.data.wallpaper.googlePhotos.albums = [album];
    personalizationStore.data.wallpaper.googlePhotos.enabled =
        GooglePhotosEnablementState.kEnabled;
    personalizationStore.data.wallpaper.googlePhotos.photosByAlbumId[album.id] =
        null;

    // Initialize |googlePhotosPhotosByAlbumIdElement| in hidden state.
    googlePhotosPhotosByAlbumIdElement =
        initElement(GooglePhotosPhotosByAlbumIdElement, {hidden: true});
    googlePhotosPhotosByAlbumIdElement.setAttribute('album-id', album.id);
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Verify that showing |googlePhotosPhotosByAlbumIdElement| results in an
    // automatic reattempt to fetch photos for the selected album.
    assertEquals(wallpaperProvider.getCallCount('fetchGooglePhotosPhotos'), 0);
    googlePhotosPhotosByAlbumIdElement.hidden = false;
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertDeepEquals(
        await wallpaperProvider.whenCalled('fetchGooglePhotosPhotos'),
        [/*itemId=*/ null, /*albumId=*/ album.id, /*resumeToken=*/ null]);

    // Only placeholders should be present while loading.
    const selector = 'wallpaper-grid-item:not([hidden]).photo';
    const photoSelector = `${selector}:not([placeholder])`;
    const placeholderSelector = `${selector}[placeholder]`;
    assertEquals(querySelectorAll(photoSelector)!.length, 0);
    assertNotEquals(querySelectorAll(placeholderSelector)!.length, 0);
  });

  test('selects photo', async () => {
    personalizationStore.setReducersEnabled(true);

    const album: GooglePhotosAlbum = {
      id: '1',
      title: 'foo',
      photoCount: 1,
      preview: {url: 'foo.com'},
      timestamp: {internalValue: BigInt('1')},
      isShared: false,
    };

    const photo: GooglePhotosPhoto = {
      id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
      dedupKey: '2d0d1595-14af-4471-b2db-b9c8eae3a491',
      name: 'foo',
      date: {data: []},
      url: {url: 'foo.com'},
      location: 'home',
    };

    // Initialize Google Photos data in the |personalizationStore|.
    personalizationStore.data.wallpaper.googlePhotos.albums = [album];
    personalizationStore.data.wallpaper.googlePhotos
        .photosByAlbumId = {[album.id]: [photo]};

    // Initialize |googlePhotosPhotosByAlbumIdElement|.
    googlePhotosPhotosByAlbumIdElement =
        initElement(GooglePhotosPhotosByAlbumIdElement, {hidden: false});
    googlePhotosPhotosByAlbumIdElement.setAttribute('album-id', album.id);
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Verify that the expected |photo| is rendered.
    const photoSelector = 'wallpaper-grid-item:not([hidden]).photo';
    const photoEls =
        querySelectorAll(photoSelector) as WallpaperGridItemElement[];
    assertEquals(photoEls.length, 1);
    assertEquals(photoEls[0]!.src, photo.url);
    assertEquals(photoEls[0]!.primaryText, undefined);
    assertEquals(photoEls[0]!.secondaryText, undefined);

    // Select |photo| and verify selection started.
    photoEls[0]!.click();
    assertEquals(personalizationStore.data.wallpaper.loading.setImage, 1);
    assertEquals(
        personalizationStore.data.wallpaper.loading.selected.image, true);
    assertEquals(personalizationStore.data.wallpaper.pendingSelected, photo);

    // Wait for and verify hard-coded selection failure.
    const methodName = 'selectGooglePhotosPhoto';
    wallpaperProvider.selectGooglePhotosPhotoResponse = false;
    assertEquals(await wallpaperProvider.whenCalled(methodName), photo.id);
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(personalizationStore.data.wallpaper.loading.setImage, 0);
    assertEquals(
        personalizationStore.data.wallpaper.loading.selected.image, false);
    assertEquals(personalizationStore.data.wallpaper.pendingSelected, null);
  });
});

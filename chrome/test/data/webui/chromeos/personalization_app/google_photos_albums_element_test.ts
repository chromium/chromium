// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {fetchGooglePhotosAlbums, fetchGooglePhotosEnabled, fetchGooglePhotosSharedAlbums, getCountText, GooglePhotosAlbum, GooglePhotosAlbumsElement, PersonalizationActionName, PersonalizationRouterElement, SetErrorAction, WallpaperGridItemElement} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertGT, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {baseSetup, createSvgDataUrl, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('GooglePhotosAlbumsElementTest', function() {
  let googlePhotosAlbumsElement: GooglePhotosAlbumsElement|null;
  let personalizationStore: TestPersonalizationStore;
  let wallpaperProvider: TestWallpaperProvider;

  /**
   * Returns all matches for |selector| in |googlePhotosAlbumsElement|'s shadow
   * DOM.
   */
  function querySelectorAll(selector: string): Element[]|null {
    const matches =
        googlePhotosAlbumsElement!.shadowRoot!.querySelectorAll(selector);
    return matches ? [...matches] : null;
  }

  function getAlbumAriaLabel(album: GooglePhotosAlbum|undefined): string {
    if (!album) {
      return '';
    }
    const primaryText = album.title;
    const secondaryText = album.isShared ?
        loadTimeData.getString('googlePhotosAlbumShared') :
        getCountText(album.photoCount);
    return `${primaryText} ${secondaryText}`;
  }

  /** Scrolls the window to the bottom. */
  async function scrollToBottom() {
    window.scroll({top: document.body.scrollHeight, behavior: 'smooth'});
    await waitAfterNextRender(googlePhotosAlbumsElement!);
  }

  setup(() => {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    personalizationStore.setReducersEnabled(true);
    wallpaperProvider = mocks.wallpaperProvider;
  });

  teardown(async () => {
    await teardownElement(googlePhotosAlbumsElement);
    googlePhotosAlbumsElement = null;
  });

  test('displays albums', async () => {
    const albums: GooglePhotosAlbum[] = [
      {
        id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
        title: 'Album 0',
        photoCount: 0,
        preview: {
          url: createSvgDataUrl('svg-0'),
        },
        timestamp: {internalValue: BigInt(`13318040939308000`)},
        isShared: false,
      },
      {
        id: '0ec40478-9712-42e1-b5bf-3e75870ca042',
        title: 'Album 1',
        photoCount: 1,
        preview: {
          url: createSvgDataUrl('svg-1'),
        },
        timestamp: {internalValue: BigInt(`13318040939307000`)},
        isShared: false,
      },
      {
        id: '0a268a37-877a-4936-81d4-38cc84b0f596',
        title: 'Album 2',
        photoCount: 2,
        preview: {
          url: createSvgDataUrl('svg-2'),
        },
        timestamp: {internalValue: BigInt(`13318040939306000`)},
        isShared: false,
      },
    ];

    // Set values returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosAlbums(albums);

    // Initialize |googlePhotosAlbumsElement|.
    googlePhotosAlbumsElement =
        initElement(GooglePhotosAlbumsElement, {hidden: false});
    await waitAfterNextRender(googlePhotosAlbumsElement);

    // The |personalizationStore| should be empty, so no albums should be
    // rendered initially.
    const albumSelector =
        'wallpaper-grid-item:not([hidden]).album:not([placeholder])';
    assertEquals(
        querySelectorAll(albumSelector)!.length, 0,
        'no wallpaper grid items yet');

    // Initialize Google Photos data in the |personalizationStore|.
    await fetchGooglePhotosEnabled(wallpaperProvider, personalizationStore);
    await fetchGooglePhotosAlbums(wallpaperProvider, personalizationStore);
    await waitAfterNextRender(googlePhotosAlbumsElement);

    // The wallpaper controller is expected to impose max resolution.
    albums.forEach(album => album.preview.url += '=s512');

    // Verify that the expected |albums| are rendered.
    const albumEls =
        querySelectorAll(albumSelector) as WallpaperGridItemElement[];

    assertEquals(
        albumEls.length, albums.length, 'one wallpaper grid item per album');
    albumEls.forEach((albumEl, i) => {
      assertDeepEquals(albumEl.src, albums[i]!.preview);
      assertEquals(albumEl.primaryText, albums[i]!.title);
      assertEquals(albumEl.secondaryText, getCountText(albums[i]!.photoCount));
    });
  });


  [true, false].forEach(
      (dismissFromUser:
           boolean) => test('displays error when albums fail to load', async () => {
        // Set values returned by |wallpaperProvider|.
        wallpaperProvider.setGooglePhotosAlbums(null);
        wallpaperProvider.setGooglePhotosSharedAlbums(null);

        // Initialize |googlePhotosAlbumsElement|.
        googlePhotosAlbumsElement =
            initElement(GooglePhotosAlbumsElement, {hidden: false});
        await waitAfterNextRender(googlePhotosAlbumsElement);

        // Initialize Google Photos data in the |personalizationStore| and
        // expect an |error|.
        personalizationStore.expectAction(PersonalizationActionName.SET_ERROR);
        await fetchGooglePhotosEnabled(wallpaperProvider, personalizationStore);
        await fetchGooglePhotosAlbums(wallpaperProvider, personalizationStore);
        await fetchGooglePhotosSharedAlbums(
            wallpaperProvider, personalizationStore);
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
            wallpaperProvider.getCallCount('fetchGooglePhotosAlbums'),
            dismissFromUser ? 1 : 0);

        wallpaperProvider.reset();

        // Simulate hiding |googlePhotosAlbumsElement| and verify the
        // |error| is dismissed though not |fromUser|.
        const dismissCallbackPromise = new Promise<boolean>(resolve => {
          personalizationStore.data.error!.dismiss!.callback = resolve;
        });
        googlePhotosAlbumsElement.hidden = true;
        assertEquals(await dismissCallbackPromise, /*fromUser=*/ false);
        await new Promise<void>(resolve => setTimeout(resolve));
        assertEquals(
            wallpaperProvider.getCallCount('fetchGooglePhotosAlbums'), 0);
      }));

  test('displays placeholders until albums are present', async () => {
    // Prepare Google Photos data.
    const photosCount = 5;
    const albums: GooglePhotosAlbum[] = Array.from(
        {length: photosCount},
        (_, i) => ({
          id: `id-${i}`,
          title: `title-${i}`,
          photoCount: 1,
          preview: {url: createSvgDataUrl(`svg-${i}`)},
          timestamp: {internalValue: BigInt(`${photosCount - i}`)},
          isShared: false,
        }));

    // Initialize |googlePhotosAlbumsElement|.
    googlePhotosAlbumsElement =
        initElement(GooglePhotosAlbumsElement, {hidden: false});
    await waitAfterNextRender(googlePhotosAlbumsElement);

    // Initially only placeholders should be present.
    const selector = 'wallpaper-grid-item:not([hidden]).album';
    const albumSelector = `${selector}:not([placeholder])`;
    const placeholderSelector = `${selector}[placeholder]`;
    const albumListSelector = 'iron-list:not([hidden])#grid';
    assertEquals(querySelectorAll(albumSelector)!.length, 0);
    const placeholderEls = querySelectorAll(placeholderSelector);
    assertGT(placeholderEls!.length, 0, 'some placeholders are shown');
    let albumListEl = querySelectorAll(albumListSelector);
    assertEquals(albumListEl!.length, 1);
    assertEquals(
        albumListEl![0]!.getAttribute('aria-setsize'),
        placeholderEls!.length.toString());

    // Placeholders should be aria-labeled.
    placeholderEls!.forEach(placeholderEl => {
      assertEquals(placeholderEl.getAttribute('aria-label'), 'Loading');
    });

    // Mock singleton |PersonalizationRouter|.
    const router = TestMock.fromClass(PersonalizationRouterElement);
    PersonalizationRouterElement.instance = () => router;

    // Mock |PersonalizationRouter.selectGooglePhotosAlbum()|.
    let selectedGooglePhotosAlbum: GooglePhotosAlbum|undefined;
    router.selectGooglePhotosAlbum = (album: GooglePhotosAlbum) => {
      selectedGooglePhotosAlbum = album;
    };

    // Clicking a placeholder should do nothing.
    (placeholderEls![0] as HTMLElement).click();
    await new Promise<void>(resolve => setTimeout(resolve));
    assertEquals(selectedGooglePhotosAlbum, undefined);

    // Provide Google Photos data.
    personalizationStore.data.wallpaper.googlePhotos.albums = albums;
    personalizationStore.notifyObservers();

    // Only albums should be present.
    await waitAfterNextRender(googlePhotosAlbumsElement);
    const albumEls = querySelectorAll(albumSelector);
    assertGT(albumEls!.length, 0, 'some album elements should be shown');
    assertEquals(querySelectorAll(placeholderSelector)!.length, 0);

    // The album list's aria-setsize should be consistent with the number of
    // albums.
    albumListEl = querySelectorAll(albumListSelector);
    assertEquals(albumListEl!.length, 1);
    assertEquals(
        albumListEl![0]!.getAttribute('aria-setsize'),
        albums.length.toString());

    // Albums should be aria-labeled.
    albumEls!.forEach((albumEl, i) => {
      assertEquals(
          albumEl.getAttribute('aria-label'), getAlbumAriaLabel(albums[i]));
      assertEquals(albumEl.getAttribute('aria-posinset'), (i + 1).toString());
    });

    // Clicking an album should do something.
    (albumEls![0] as HTMLElement).click();
    assertEquals(selectedGooglePhotosAlbum, albums[0]);
  });

  test('incrementally loads albums', async () => {
    // Set initial list of albums returned by |wallpaperProvider|.
    const albumsCount = 200;
    let nextAlbumId = 1;
    wallpaperProvider.setGooglePhotosAlbums(
        Array.from({length: albumsCount / 2}).map(() => {
          return {
            id: `id-${nextAlbumId}`,
            title: `title-${nextAlbumId}`,
            photoCount: 1,
            preview: {url: `url-${nextAlbumId++}`},
            timestamp: {internalValue: BigInt(`${nextAlbumId}`)},
            isShared: false,
          };
        }));

    // Set initial albums resume token returned by |wallpaperProvider|. When
    // resume token is defined, it indicates additional albums exist.
    const resumeToken = 'resumeToken';
    wallpaperProvider.setGooglePhotosAlbumsResumeToken(resumeToken);

    // Initialize Google Photos data in |personalizationStore|.
    await fetchGooglePhotosEnabled(wallpaperProvider, personalizationStore);
    await fetchGooglePhotosAlbums(wallpaperProvider, personalizationStore);
    assertEquals(
        await wallpaperProvider.whenCalled('fetchGooglePhotosAlbums'),
        /*resumeToken=*/ null);

    // Reset |wallpaperProvider| expectations.
    wallpaperProvider.resetResolver('fetchGooglePhotosAlbums');

    // Set the next list of albums returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosAlbums(
        Array.from({length: albumsCount / 2}).map(() => {
          return {
            id: `id-${nextAlbumId}`,
            title: `title-${nextAlbumId}`,
            photoCount: 1,
            preview: {url: `url-${nextAlbumId++}`},
            timestamp: {internalValue: BigInt(`${nextAlbumId}`)},
            isShared: false,
          };
        }));

    // Set the next albums resume token returned by |wallpaperProvider|. When
    // resume token is null, it indicates no additional albums exist.
    wallpaperProvider.setGooglePhotosAlbumsResumeToken(null);

    // Restrict the viewport so that |googlePhotosAlbumsElement| will lazily
    // create albums instead of creating them all at once.
    const style = document.createElement('style');
    style.appendChild(document.createTextNode(`
      html,
      body {
        height: 100%;
        width: 100%;
      }
    `));
    document.head.appendChild(style);

    // Initialize |googlePhotosAlbumsElement|.
    googlePhotosAlbumsElement =
        initElement(GooglePhotosAlbumsElement, {hidden: false});
    await waitAfterNextRender(googlePhotosAlbumsElement);

    // Scroll to the bottom of the grid.
    scrollToBottom();

    // Wait for and verify that the next batch of albums has been
    // requested.
    assertEquals(
        await wallpaperProvider.whenCalled('fetchGooglePhotosAlbums'),
        resumeToken);
    await waitAfterNextRender(googlePhotosAlbumsElement);

    // Reset |wallpaperProvider| expectations.
    wallpaperProvider.resetResolver('fetchGooglePhotosAlbums');

    // Scroll to the bottom of the grid.
    scrollToBottom();

    // Verify that no next batch of albums has been requested.
    assertEquals(wallpaperProvider.getCallCount('fetchGooglePhotosAlbums'), 0);
  });
});

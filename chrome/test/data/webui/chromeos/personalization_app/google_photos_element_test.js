// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GooglePhotos} from 'chrome://personalization/trusted/google_photos_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {waitAfterNextRender} from '../../test_util.js';
import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

export function GooglePhotosTest() {
  /** @type {?HTMLElement} */
  let googlePhotosElement = null;

  /** @type {?TestPersonalizationStore} */
  let personalizationStore = null;

  /**
   * Returns the match for |selector| in |googlePhotosElement|'s shadow DOM.
   * @return {?Element|undefined}
   */
  function querySelector(selector) {
    return googlePhotosElement?.shadowRoot?.querySelector(selector);
  }

  setup(() => {
    // Google Photos strings are only supplied when the Google Photos
    // integration feature flag is enabled.
    loadTimeData.overrideValues({
      'googlePhotosLabel': 'Google Photos',
      'googlePhotosAlbumsTabLabel': 'Albums',
      'googlePhotosPhotosTabLabel': 'Photos',
    });

    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
  });

  teardown(async () => {
    await teardownElement(googlePhotosElement);
    googlePhotosElement = null;
  });

  test('displays only photos content', async () => {
    // Tabs and albums content are not displayed if albums are absent.
    personalizationStore.data.googlePhotos.albums = null;
    personalizationStore.data.loading.googlePhotos.albums = false;

    googlePhotosElement = initElement(GooglePhotos.is, {hidden: false});
    await waitAfterNextRender(googlePhotosElement);

    // Tabs should be absent.
    assertEquals(querySelector('.tabStrip'), null);

    // Photos content should be present and visible.
    const photosContent = querySelector('#photosContent');
    assertTrue(!!photosContent);
    assertFalse(photosContent.hidden);

    // Albums content should be absent.
    assertEquals(querySelector('#albumsContent'), null);
  });

  test('displays tabs and content for photos and albums', async () => {
    // Tabs and albums content are only displayed if albums are present.
    personalizationStore.data.googlePhotos.albums = Array.from({length: 1});
    personalizationStore.data.loading.googlePhotos.albums = false;

    googlePhotosElement = initElement(GooglePhotos.is, {hidden: false});
    await waitAfterNextRender(googlePhotosElement);

    // Photos tab should be present, visible, and pressed.
    const photosTab = querySelector('#photosTab');
    assertTrue(!!photosTab);
    assertFalse(photosTab.hidden);
    assertEquals(photosTab.getAttribute('aria-pressed'), 'true');

    // Photos content should be present and visible.
    const photosContent = querySelector('#photosContent');
    assertTrue(!!photosContent);
    assertFalse(photosContent.hidden);

    // Albums tab should be present, visible, and *not* pressed.
    const albumsTab = querySelector('#albumsTab');
    assertTrue(!!albumsTab);
    assertFalse(albumsTab.hidden);
    assertEquals(albumsTab.getAttribute('aria-pressed'), 'false');

    // Albums content should be present and hidden.
    const albumsContent = querySelector('#albumsContent');
    assertTrue(!!albumsContent);
    assertTrue(albumsContent.hidden);

    // Clicking the albums tab should cause:
    // * albums tab to be visible and pressed.
    // * albums content to be visible.
    // * photos tab to be visible and *not* pressed.
    // * photos content to be hidden.
    albumsTab.click();
    assertFalse(albumsTab.hidden);
    assertEquals(albumsTab.getAttribute('aria-pressed'), 'true');
    assertFalse(albumsContent.hidden);
    assertFalse(photosTab.hidden);
    assertEquals(photosTab.getAttribute('aria-pressed'), 'false');
    assertTrue(photosContent.hidden);

    // Clicking the photos tab should cause:
    // * photos tab to be visible and pressed.
    // * photos content to be visible.
    // * albums tab to be visible and *not* pressed.
    // * albums content to be visible.
    photosTab.click();
    assertFalse(photosTab.hidden);
    assertEquals(photosTab.getAttribute('aria-pressed'), 'true');
    assertFalse(photosContent.hidden);
    assertFalse(albumsTab.hidden);
    assertEquals(albumsTab.getAttribute('aria-pressed'), 'false');
    assertTrue(albumsContent.hidden);
  });
}

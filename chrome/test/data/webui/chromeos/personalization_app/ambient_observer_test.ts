// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {AmbientActionName, AmbientModeAlbum, AmbientObserver, emptyState, SetAlbumsAction, TopicSource} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {baseSetup} from './personalization_app_test_utils.js';
import {TestAmbientProvider} from './test_ambient_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

suite('AmbientObserverTest', function() {
  let ambientProvider: TestAmbientProvider;
  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    loadTimeData.overrideValues({isAmbientModeAllowed: true});
    const mocks = baseSetup();
    ambientProvider = mocks.ambientProvider;
    personalizationStore = mocks.personalizationStore;
    AmbientObserver.initAmbientObserverIfNeeded();
  });

  teardown(() => {
    AmbientObserver.shutdown();
  });

  test('requests fetchSettingsAndAlbums on first load', async () => {
    await ambientProvider.whenCalled('fetchSettingsAndAlbums');
  });

  test('sets albums in store', async () => {
    personalizationStore.setReducersEnabled(true);
    // Make sure state starts as expected.
    assertDeepEquals(emptyState(), personalizationStore.data);
    assertEquals(null, personalizationStore.data.ambient.albums);

    personalizationStore.expectAction(AmbientActionName.SET_ALBUMS);
    ambientProvider.ambientObserverRemote!.onAlbumsChanged(
        ambientProvider.albums);

    const {albums} = await personalizationStore.waitForAction(
                         AmbientActionName.SET_ALBUMS) as SetAlbumsAction;

    assertDeepEquals(ambientProvider.albums, albums);
  });

  test('keeps recent highlights preview image', async () => {
    const initialAlbums: AmbientModeAlbum[] = [
      {
        id: 'RecentHighlights',
        checked: false,
        numberOfPhotos: 210,
        title: 'Recent Highlights title',
        description: 'Recent Highlights description',
        topicSource: TopicSource.kGooglePhotos,
        url: {url: 'asdf'},
      },
      {
        id: 'abcdef',
        checked: false,
        numberOfPhotos: 3,
        title: 'Another album',
        description: 'Another album description',
        topicSource: TopicSource.kGooglePhotos,
        url: {url: 'qwerty'},
      },
    ];
    personalizationStore.data.ambient.albums = initialAlbums;

    personalizationStore.expectAction(AmbientActionName.SET_ALBUMS);

    ambientProvider.ambientObserverRemote!.onAlbumsChanged([
      {
        ...initialAlbums[0]!,
        url: {
          url: 'new-recent-highlights-url',
        },
      },
      {
        ...initialAlbums[1]!,
        url: {
          url: 'new-regular-album-url',
        },
      },
    ]);

    const {albums} = await personalizationStore.waitForAction(
                         AmbientActionName.SET_ALBUMS) as SetAlbumsAction;

    assertEquals('RecentHighlights', albums[0]!.id);
    assertEquals(
        'asdf', albums[0]!.url.url, 'kept original url for recent highlights');
    assertEquals(
        'new-regular-album-url', albums[1]!.url.url,
        'used updated regular album url');
  });
});

suite('GooglePhotosPreviewLoadPerformance', () => {
  let ambientProvider: TestAmbientProvider;
  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    const mocks = baseSetup();
    ambientProvider = mocks.ambientProvider;
    personalizationStore = mocks.personalizationStore;
    // Reset the value to always start true in test.
    AmbientObserver.shouldLogPreviewsLoadPerformance = true;
    AmbientObserver.initAmbientObserverIfNeeded();
  });

  teardown(() => {
    AmbientObserver.shutdown();
  });

  test('sets to false if ambient mode disabled', async () => {
    ambientProvider.ambientObserverRemote!.onAmbientModeEnabledChanged(false);
    personalizationStore.expectAction(
        AmbientActionName.SET_AMBIENT_MODE_ENABLED);
    await personalizationStore.waitForAction(
        AmbientActionName.SET_AMBIENT_MODE_ENABLED);
    assertFalse(AmbientObserver.shouldLogPreviewsLoadPerformance);
  });

  test('sets to false if already received preview images', async () => {
    personalizationStore.data.ambient.previews = [];
    ambientProvider.ambientObserverRemote!.onPreviewsFetched([]);
    personalizationStore.expectAction(AmbientActionName.SET_PREVIEWS);
    await personalizationStore.waitForAction(AmbientActionName.SET_PREVIEWS);
    assertTrue(
        AmbientObserver.shouldLogPreviewsLoadPerformance,
        'still true because no previews stored yet');

    personalizationStore.data.ambient.previews = [
      {url: 'asdf'},
    ];
    ambientProvider.ambientObserverRemote!.onPreviewsFetched([]);
    personalizationStore.expectAction(AmbientActionName.SET_PREVIEWS);
    await personalizationStore.waitForAction(AmbientActionName.SET_PREVIEWS);
    assertFalse(AmbientObserver.shouldLogPreviewsLoadPerformance);
  });

  test('sets to false after receiving preview images', async () => {
    personalizationStore.data.ambient.previews = [];
    ambientProvider.ambientObserverRemote!.onPreviewsFetched([
      {url: 'asdf'},
    ]);
    personalizationStore.expectAction(AmbientActionName.SET_PREVIEWS);
    await personalizationStore.waitForAction(AmbientActionName.SET_PREVIEWS);
    assertFalse(AmbientObserver.shouldLogPreviewsLoadPerformance);
  });
});

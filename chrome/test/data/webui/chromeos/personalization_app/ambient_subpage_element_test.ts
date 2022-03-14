// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AlbumItem} from 'chrome://personalization/trusted/ambient/album_item_element.js';
import {AlbumsSubpage} from 'chrome://personalization/trusted/ambient/albums_subpage_element.js';
import {AmbientActionName, SetAlbumsAction, SetAmbientModeEnabledAction, SetTemperatureUnitAction, SetTopicSourceAction} from 'chrome://personalization/trusted/ambient/ambient_actions.js';
import {AmbientObserver} from 'chrome://personalization/trusted/ambient/ambient_observer.js';
import {AmbientSubpage} from 'chrome://personalization/trusted/ambient/ambient_subpage_element.js';
import {TopicSourceItem} from 'chrome://personalization/trusted/ambient/topic_source_item_element.js';
import {AmbientModeAlbum, TemperatureUnit, TopicSource} from 'chrome://personalization/trusted/personalization_app.mojom-webui.js';
import {Paths, PersonalizationRouter} from 'chrome://personalization/trusted/personalization_router_element.js';
import {emptyState} from 'chrome://personalization/trusted/personalization_state.js';
import {CrRadioButtonElement} from 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestAmbientProvider} from './test_ambient_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';


export function getSelectedAlbums(
    albums: AmbientModeAlbum[], topicSource: TopicSource): AmbientModeAlbum[] {
  return albums.filter(
      album => album.topicSource === topicSource && album.checked);
}

export function AmbientSubpageTest() {
  let ambientSubpageElement: AmbientSubpage|null;
  let ambientProvider: TestAmbientProvider;
  let personalizationStore: TestPersonalizationStore;
  const routerOriginal = PersonalizationRouter.instance;
  const routerMock = TestBrowserProxy.fromClass(PersonalizationRouter);

  setup(() => {
    const mocks = baseSetup();
    ambientProvider = mocks.ambientProvider;
    personalizationStore = mocks.personalizationStore;
    AmbientObserver.initAmbientObserverIfNeeded();
    PersonalizationRouter.instance = () => routerMock;
  });

  teardown(async () => {
    await teardownElement(ambientSubpageElement);
    ambientSubpageElement = null;
    AmbientObserver.shutdown();
    PersonalizationRouter.instance = routerOriginal;
  });

  async function displayMainSettings(
      topicSource: TopicSource|null,
      temperatureUnit: TemperatureUnit|null): Promise<AmbientSubpage> {
    personalizationStore.data.ambient.albums = ambientProvider.albums;
    personalizationStore.data.ambient.topicSource = topicSource;
    personalizationStore.data.ambient.temperatureUnit = temperatureUnit;
    personalizationStore.data.ambient.ambientModeEnabled = false;
    const ambientSubpage =
        initElement(AmbientSubpage, {path: Paths.Ambient, queryParams: {}});
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientSubpage);
    return Promise.resolve(ambientSubpage);
  }

  test('displays content', async () => {
    ambientSubpageElement = await displayMainSettings(null, null);

    const toggleRow =
        ambientSubpageElement.shadowRoot!.querySelector('toggle-row');
    assertTrue(!!toggleRow, 'toggle-row element exists');
    const toggleButton = toggleRow!.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!toggleButton, 'cr-toggle element exists');
    assertFalse(toggleButton!.checked);

    let spinner =
        ambientSubpageElement.shadowRoot!.querySelector('paper-spinner-lite');
    assertTrue(!!spinner, 'paper-spinner-lite element exists');
    assertTrue(spinner.active, 'paper-spinner-lite is active');

    personalizationStore.data.ambient.albums = ambientProvider.albums;
    personalizationStore.data.ambient.topicSource = TopicSource.kGooglePhotos;
    personalizationStore.data.ambient.temperatureUnit =
        TemperatureUnit.kFahrenheit;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientSubpageElement);

    spinner =
        ambientSubpageElement.shadowRoot!.querySelector('paper-spinner-lite');
    assertTrue(!!spinner, 'paper-spinner-lite still exists');
    assertEquals(getComputedStyle(spinner).display, 'none');

    const topicSource =
        ambientSubpageElement.shadowRoot!.querySelector('topic-source-list');
    assertTrue(!!topicSource, 'topic-source-list element exists');

    const weatherUnit =
        ambientSubpageElement.shadowRoot!.querySelector('ambient-weather-unit');
    assertTrue(!!weatherUnit);
  });

  test('sets ambient mode enabled in store on first load', async () => {
    personalizationStore.expectAction(
        AmbientActionName.SET_AMBIENT_MODE_ENABLED);
    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit);

    await ambientProvider.whenCalled('setAmbientObserver');
    ambientProvider.updateAmbientObserver();

    const action = await personalizationStore.waitForAction(
                       AmbientActionName.SET_AMBIENT_MODE_ENABLED) as
        SetAmbientModeEnabledAction;
    assertTrue(action.enabled);
  });

  test('sets ambient mode when pref value changed', async () => {
    // Make sure state starts as expected.
    assertDeepEquals(emptyState(), personalizationStore.data);
    ambientSubpageElement = initElement(AmbientSubpage);
    await ambientProvider.whenCalled('setAmbientObserver');

    personalizationStore.expectAction(
        AmbientActionName.SET_AMBIENT_MODE_ENABLED);
    ambientProvider.ambientObserverRemote!.onAmbientModeEnabledChanged(
        /*ambientModeEnabled=*/ false);

    const action = await personalizationStore.waitForAction(
                       AmbientActionName.SET_AMBIENT_MODE_ENABLED) as
        SetAmbientModeEnabledAction;
    assertFalse(action.enabled);
  });

  test('sets ambient mode enabled when toggle row clicked', async () => {
    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit);
    personalizationStore.data.ambient.ambientModeEnabled = true;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientSubpageElement);

    const toggleRow =
        ambientSubpageElement.shadowRoot!.querySelector('toggle-row');
    assertTrue(!!toggleRow);
    const toggleButton = toggleRow!.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!toggleButton);
    assertTrue(toggleButton!.checked);

    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(
        AmbientActionName.SET_AMBIENT_MODE_ENABLED);
    toggleRow.click();
    let action = await personalizationStore.waitForAction(
                     AmbientActionName.SET_AMBIENT_MODE_ENABLED) as
        SetAmbientModeEnabledAction;
    assertFalse(action.enabled);
    assertFalse(personalizationStore.data.ambient.ambientModeEnabled);
    assertFalse(toggleButton!.checked);

    personalizationStore.expectAction(
        AmbientActionName.SET_AMBIENT_MODE_ENABLED);
    toggleRow.click();
    action = await personalizationStore.waitForAction(
                 AmbientActionName.SET_AMBIENT_MODE_ENABLED) as
        SetAmbientModeEnabledAction;
    assertTrue(action.enabled);
    assertTrue(personalizationStore.data.ambient.ambientModeEnabled);
    assertTrue(toggleButton!.checked);
  });

  test('sets ambient mode enabled when toggle button clicked', async () => {
    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit);
    personalizationStore.data.ambient.ambientModeEnabled = true;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientSubpageElement);

    const toggleRow =
        ambientSubpageElement.shadowRoot!.querySelector('toggle-row');
    assertTrue(!!toggleRow);
    const toggleButton = toggleRow!.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!toggleButton);
    assertTrue(toggleButton!.checked);

    personalizationStore.expectAction(
        AmbientActionName.SET_AMBIENT_MODE_ENABLED);
    toggleRow.$.toggle.click();
    let action = await personalizationStore.waitForAction(
                     AmbientActionName.SET_AMBIENT_MODE_ENABLED) as
        SetAmbientModeEnabledAction;
    assertFalse(action.enabled);

    personalizationStore.expectAction(
        AmbientActionName.SET_AMBIENT_MODE_ENABLED);
    toggleRow.$.toggle.click();
    action = await personalizationStore.waitForAction(
                 AmbientActionName.SET_AMBIENT_MODE_ENABLED) as
        SetAmbientModeEnabledAction;
    assertTrue(action.enabled);
  });

  test('has correct topic sources on load', async () => {
    personalizationStore.expectAction(AmbientActionName.SET_TOPIC_SOURCE);
    ambientSubpageElement = initElement(AmbientSubpage);

    await ambientProvider.whenCalled('setAmbientObserver');
    ambientProvider.updateAmbientObserver();

    const action =
        await personalizationStore.waitForAction(
            AmbientActionName.SET_TOPIC_SOURCE) as SetTopicSourceAction;
    assertEquals(TopicSource.kArtGallery, action.topicSource);
  });

  test('sets topic source when topic source item clicked', async () => {
    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit);

    const topicSourceList =
        ambientSubpageElement.shadowRoot!.querySelector('topic-source-list');
    assertTrue(!!topicSourceList);
    const topicSourceItems =
        topicSourceList!.shadowRoot!.querySelectorAll('topic-source-item');
    assertEquals(2, topicSourceItems!.length);
    const googlePhotos = topicSourceItems[0] as TopicSourceItem;
    const art = topicSourceItems[1] as TopicSourceItem;
    assertEquals(TopicSource.kGooglePhotos, googlePhotos.topicSource);
    assertEquals(TopicSource.kArtGallery, art.topicSource);

    assertFalse(googlePhotos.checked);
    assertTrue(art.checked);

    personalizationStore.expectAction(AmbientActionName.SET_TOPIC_SOURCE);
    googlePhotos!.click();
    let action =
        await personalizationStore.waitForAction(
            AmbientActionName.SET_TOPIC_SOURCE) as SetTopicSourceAction;
    assertEquals(TopicSource.kGooglePhotos, action.topicSource);

    personalizationStore.expectAction(AmbientActionName.SET_TOPIC_SOURCE);
    art!.click();
    action = await personalizationStore.waitForAction(
                 AmbientActionName.SET_TOPIC_SOURCE) as SetTopicSourceAction;
    assertEquals(TopicSource.kArtGallery, action.topicSource);
  });

  test('has correct temperature unit on load', async () => {
    personalizationStore.expectAction(AmbientActionName.SET_TEMPERATURE_UNIT);
    ambientSubpageElement = initElement(AmbientSubpage);

    await ambientProvider.whenCalled('setAmbientObserver');
    ambientProvider.updateAmbientObserver();

    const action =
        await personalizationStore.waitForAction(
            AmbientActionName.SET_TEMPERATURE_UNIT) as SetTemperatureUnitAction;
    assertEquals(TemperatureUnit.kFahrenheit, action.temperatureUnit);
  });

  test('sets temperature unit when temperature unit item clicked', async () => {
    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit);

    const weatherUnit =
        ambientSubpageElement.shadowRoot!.querySelector('ambient-weather-unit');
    assertTrue(!!weatherUnit);

    const temperatureUnitItems =
        weatherUnit!.shadowRoot!.querySelectorAll<CrRadioButtonElement>(
            'cr-radio-button');
    assertEquals(2, temperatureUnitItems!.length);

    const [fahrenheitUnitButton, celsiusUnitButton] = temperatureUnitItems;

    assertTrue(
        fahrenheitUnitButton!.checked, 'fahrenheit button starts checked');

    personalizationStore.expectAction(AmbientActionName.SET_TEMPERATURE_UNIT);
    celsiusUnitButton!.click();
    assertTrue(celsiusUnitButton!.checked);
    let action =
        await personalizationStore.waitForAction(
            AmbientActionName.SET_TEMPERATURE_UNIT) as SetTemperatureUnitAction;
    assertEquals(TemperatureUnit.kCelsius, action.temperatureUnit);

    personalizationStore.expectAction(AmbientActionName.SET_TEMPERATURE_UNIT);
    fahrenheitUnitButton!.click();
    assertTrue(fahrenheitUnitButton!.checked);
    action =
        await personalizationStore.waitForAction(
            AmbientActionName.SET_TEMPERATURE_UNIT) as SetTemperatureUnitAction;
    assertEquals(TemperatureUnit.kFahrenheit, action.temperatureUnit);
  });

  test('has main settings visible with path ambient', async () => {
    ambientSubpageElement =
        initElement(AmbientSubpage, {path: Paths.Ambient, queryParams: {}});
    await waitAfterNextRender(ambientSubpageElement);

    const mainSettings =
        ambientSubpageElement.shadowRoot!.querySelector<HTMLElement>(
            '#mainSettings');
    assertTrue(!!mainSettings);
    assertFalse(mainSettings.hidden);

    const albumsSubpage =
        ambientSubpageElement.shadowRoot!.querySelector('albums-subpage');
    assertFalse(!!albumsSubpage);
  });

  test('has albums subpage visible with path ambient albums', async () => {
    ambientSubpageElement = initElement(AmbientSubpage, {
      path: Paths.AmbientAlbums,
      queryParams: {topicSource: TopicSource.kArtGallery}
    });
    personalizationStore.data.ambient.ambientModeEnabled = true;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientSubpageElement);

    const mainSettings =
        ambientSubpageElement.shadowRoot!.querySelector<HTMLElement>(
            '#mainSettings');
    assertFalse(!!mainSettings);

    const albumsSubpage =
        ambientSubpageElement.shadowRoot!.querySelector('albums-subpage');
    assertTrue(!!albumsSubpage);
    assertFalse(albumsSubpage.hidden);
  });

  test(
      'loading albums subpage redirects to ambient subpage if disabled',
      async () => {
        const reloadCalledPromise = new Promise<void>((resolve) => {
          PersonalizationRouter.reloadAtAmbient = resolve;
        });
        initElement(AlbumsSubpage, {
          path: Paths.AmbientAlbums,
          disabled: true,
        });

        await reloadCalledPromise;
      });

  test('has spinner when no albums on albums subpage', async () => {
    ambientSubpageElement = initElement(AmbientSubpage, {
      path: Paths.AmbientAlbums,
      queryParams: {topicSource: TopicSource.kGooglePhotos}
    });
    personalizationStore.data.ambient.ambientModeEnabled = true;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientSubpageElement);

    const albumsSubpage =
        ambientSubpageElement.shadowRoot!.querySelector('albums-subpage');
    assertTrue(!!albumsSubpage);
    assertFalse(albumsSubpage.hidden);
    await waitAfterNextRender(albumsSubpage);

    let spinner = albumsSubpage.shadowRoot!.querySelector('paper-spinner-lite');
    assertTrue(!!spinner);
    assertTrue(spinner.active);

    personalizationStore.data.ambient.albums = ambientProvider.albums;
    personalizationStore.data.ambient.topicSource = TopicSource.kGooglePhotos;
    personalizationStore.data.ambient.temperatureUnit =
        TemperatureUnit.kFahrenheit;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(albumsSubpage);

    spinner = albumsSubpage.shadowRoot!.querySelector('paper-spinner-lite');
    assertTrue(!!spinner);
    assertEquals(getComputedStyle(spinner).display, 'none');
  });

  test('has correct albums on Google Photos albums subpage', async () => {
    ambientSubpageElement = initElement(AmbientSubpage, {
      path: Paths.AmbientAlbums,
      queryParams: {topicSource: TopicSource.kGooglePhotos}
    });
    personalizationStore.data.ambient.ambientModeEnabled = true;
    personalizationStore.data.ambient.albums = ambientProvider.albums;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientSubpageElement);

    const albumsSubpage =
        ambientSubpageElement.shadowRoot!.querySelector('albums-subpage');
    assertTrue(!!albumsSubpage);
    assertFalse(albumsSubpage.hidden);
    await waitAfterNextRender(albumsSubpage);

    const albumList = albumsSubpage.shadowRoot!.querySelector('album-list');
    assertTrue(!!albumList);

    const albums = albumList.shadowRoot!.querySelectorAll<AlbumItem>(
        'album-item:not([hidden])');
    assertEquals(1, albums.length);
    assertTrue(!!albums[0]);
    assertEquals('3', albums[0].album!.id);
    assertFalse(albums[0].album!.checked);
    assertEquals(1, albums[0].album!.numberOfPhotos);
    assertEquals(TopicSource.kGooglePhotos, albums[0].album!.topicSource);
  });

  test('has correct albums on Art albums subpage', async () => {
    ambientSubpageElement = initElement(AmbientSubpage, {
      path: Paths.AmbientAlbums,
      queryParams: {topicSource: TopicSource.kArtGallery}
    });
    personalizationStore.data.ambient.ambientModeEnabled = true;
    personalizationStore.data.ambient.albums = ambientProvider.albums;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientSubpageElement);

    const albumsSubpage =
        ambientSubpageElement.shadowRoot!.querySelector('albums-subpage');
    assertTrue(!!albumsSubpage);
    assertFalse(albumsSubpage.hidden);
    await waitAfterNextRender(albumsSubpage);

    const albumList = albumsSubpage.shadowRoot!.querySelector('album-list');
    assertTrue(!!albumList);

    const albums = albumList.shadowRoot!.querySelectorAll<AlbumItem>(
        'album-item:not([hidden])');
    assertEquals(3, albums.length);
    assertTrue(!!albums[0]);
    assertTrue(!!albums[1]);
    assertTrue(!!albums[2]);

    assertEquals('0', albums[0].album!.id);
    assertFalse(albums[0].album!.checked);
    assertEquals(TopicSource.kArtGallery, albums[0].album!.topicSource);
    assertEquals('1', albums[1].album!.id);
    assertFalse(albums[1].album!.checked);
    assertEquals(TopicSource.kArtGallery, albums[1].album!.topicSource);
    assertEquals('2', albums[2].album!.id);
    assertTrue(albums[2].album!.checked);
    assertEquals(TopicSource.kArtGallery, albums[2].album!.topicSource);
  });

  test('toggle album selection by clicking', async () => {
    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(AmbientActionName.SET_ALBUMS);
    ambientSubpageElement = initElement(AmbientSubpage, {
      path: Paths.AmbientAlbums,
      queryParams: {topicSource: TopicSource.kArtGallery}
    });

    await ambientProvider.whenCalled('setAmbientObserver');
    ambientProvider.updateAmbientObserver();

    const action = await personalizationStore.waitForAction(
                       AmbientActionName.SET_ALBUMS) as SetAlbumsAction;
    assertEquals(4, action.albums.length);

    const albumsSubpage =
        ambientSubpageElement.shadowRoot!.querySelector('albums-subpage');
    assertTrue(!!albumsSubpage);
    assertFalse(albumsSubpage.hidden);
    await waitAfterNextRender(albumsSubpage);

    const albumList = albumsSubpage.shadowRoot!.querySelector('album-list');
    assertTrue(!!albumList);

    const albums = albumList.shadowRoot!.querySelectorAll<AlbumItem>(
        'album-item:not([hidden])');
    assertEquals(3, albums.length);
    assertTrue(!!albums[0]);
    assertTrue(!!albums[1]);
    assertTrue(!!albums[2]);
    assertFalse(albums[0].album!.checked);
    assertFalse(albums[1].album!.checked);
    assertTrue(albums[2].album!.checked);
    let selectedAlbums = getSelectedAlbums(
        personalizationStore.data.ambient.albums,
        personalizationStore.data.ambient.topicSource);
    assertEquals(1, selectedAlbums!.length);
    assertEquals('2', selectedAlbums[0]!.title);

    personalizationStore.expectAction(AmbientActionName.SET_ALBUM_SELECTED);
    albums[1].$.image.click();
    assertTrue(albums[1].album!.checked);
    await personalizationStore.waitForAction(
        AmbientActionName.SET_ALBUM_SELECTED);
    selectedAlbums = getSelectedAlbums(
        personalizationStore.data.ambient.albums,
        personalizationStore.data.ambient.topicSource);
    assertEquals(2, selectedAlbums!.length);
    assertEquals('1', selectedAlbums[0]!.title);
    assertEquals('2', selectedAlbums[1]!.title);
  });

  test('not deselect last art album', async () => {
    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(AmbientActionName.SET_ALBUMS);
    ambientSubpageElement = initElement(AmbientSubpage, {
      path: Paths.AmbientAlbums,
      queryParams: {topicSource: TopicSource.kArtGallery}
    });

    await ambientProvider.whenCalled('setAmbientObserver');
    ambientProvider.updateAmbientObserver();

    const action = await personalizationStore.waitForAction(
                       AmbientActionName.SET_ALBUMS) as SetAlbumsAction;
    assertEquals(4, action.albums.length);

    const albumsSubpage =
        ambientSubpageElement.shadowRoot!.querySelector('albums-subpage');
    assertTrue(!!albumsSubpage);
    assertFalse(albumsSubpage.hidden);
    await waitAfterNextRender(albumsSubpage);

    const albumList = albumsSubpage.shadowRoot!.querySelector('album-list');
    assertTrue(!!albumList);

    const albums = albumList.shadowRoot!.querySelectorAll<AlbumItem>(
        'album-item:not([hidden])');
    assertEquals(3, albums.length);
    assertTrue(!!albums[0]);
    assertTrue(!!albums[1]);
    assertTrue(!!albums[2]);
    assertFalse(albums[0].album!.checked);
    assertFalse(albums[1].album!.checked);
    assertTrue(albums[2].album!.checked);

    // Click the last art album item image will not toggle the check and will
    // show a dialog.
    albums[2].$.image.click();
    assertTrue(albums[2].album!.checked);

    const artAlbumDialog =
        albumsSubpage.shadowRoot!.querySelector('art-album-dialog');
    assertTrue(!!artAlbumDialog);
    await waitAfterNextRender(artAlbumDialog);
    assertTrue(artAlbumDialog.$.dialog.open);
  });

  test('has correct album preview information', async () => {
    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(AmbientActionName.SET_ALBUMS);

    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit);

    await ambientProvider.whenCalled('setAmbientObserver');
    ambientProvider.updateAmbientObserver();

    const action = await personalizationStore.waitForAction(
                       AmbientActionName.SET_ALBUMS) as SetAlbumsAction;
    assertEquals(4, action.albums.length);
    const ambientPreview =
        ambientSubpageElement.shadowRoot!.querySelector('ambient-preview');
    assertTrue(!!ambientPreview);

    const previewImage =
        ambientPreview.shadowRoot!.querySelector<HTMLImageElement>(
            '.preview-image');
    assertTrue(!!previewImage);
    assertTrue(previewImage.src.includes('test_url2'));

    const previewAlbumTitle =
        ambientPreview.shadowRoot!.getElementById('albumTitle');
    assertTrue(!!previewAlbumTitle);
    assertEquals('2', previewAlbumTitle.innerText.replace(/\s/g, ''));
  });
}

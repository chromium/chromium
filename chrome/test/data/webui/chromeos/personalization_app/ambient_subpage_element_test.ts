// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {AlbumsSubpageElement, AmbientActionName, AmbientModeAlbum, AmbientObserver, AmbientSubpageElement, AmbientTheme, AmbientThemeItemElement, AmbientUiVisibility, emptyState, Paths, PersonalizationRouterElement, QueryParams, ScrollableTarget, SetAlbumsAction, SetAmbientModeEnabledAction, SetAmbientThemeAction, SetGeolocationPermissionEnabledActionForAmbient, SetScreenSaverDurationAction, SetTemperatureUnitAction, SetTopicSourceAction, TemperatureUnit, TopicSource, TopicSourceItemElement, WallpaperGridItemElement} from 'chrome://personalization/js/personalization_app.js';
import {CrRadioButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestAmbientProvider} from './test_ambient_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';


export function getSelectedAlbums(
    albums: AmbientModeAlbum[], topicSource: TopicSource): AmbientModeAlbum[] {
  return albums.filter(
      album => album.topicSource === topicSource && album.checked);
}

suite('AmbientSubpageElementTest', function() {
  let ambientSubpageElement: AmbientSubpageElement|null;
  let ambientProvider: TestAmbientProvider;
  let personalizationStore: TestPersonalizationStore;
  const routerOriginal = PersonalizationRouterElement.instance;
  const routerMock = TestMock.fromClass(PersonalizationRouterElement);

  const enum DurationOptions {
    FIVE_MINUTES = '5',
    TEN_MINUTES = '10',
    THIRTY_MINUTES = '30',
    ONE_HOUR = '60',
    FOREVER = '0',
  }

  setup(() => {
    loadTimeData.overrideValues({isAmbientModeAllowed: true});
    const mocks = baseSetup();
    ambientProvider = mocks.ambientProvider;
    personalizationStore = mocks.personalizationStore;
    AmbientObserver.initAmbientObserverIfNeeded();
    PersonalizationRouterElement.instance = () => routerMock;
  });

  teardown(async () => {
    await teardownElement(ambientSubpageElement);
    ambientSubpageElement = null;
    AmbientObserver.shutdown();
    PersonalizationRouterElement.instance = routerOriginal;
  });

  async function displayMainSettings(
      topicSource: TopicSource|null, temperatureUnit: TemperatureUnit|null,
      ambientModeEnabled: boolean|null, ambientTheme = AmbientTheme.kSlideshow,
      previews: Url[] = [], duration: number|null = 10,
      queryParams: QueryParams = {}): Promise<AmbientSubpageElement> {
    personalizationStore.data.ambient.albums = ambientProvider.albums;
    personalizationStore.data.ambient.ambientTheme = ambientTheme;
    personalizationStore.data.ambient.topicSource = topicSource;
    personalizationStore.data.ambient.temperatureUnit = temperatureUnit;
    personalizationStore.data.ambient.ambientModeEnabled = ambientModeEnabled;
    personalizationStore.data.ambient.previews = previews;
    personalizationStore.data.ambient.duration = duration;
    const ambientSubpage =
        initElement(AmbientSubpageElement, {path: Paths.AMBIENT, queryParams});
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientSubpage);
    return Promise.resolve(ambientSubpage);
  }

  function selectDropDownMenuOption(select: HTMLSelectElement, value: string) {
    select.value = value;
    select.dispatchEvent(new CustomEvent('change'));
    flush();
  }

  test('displays content', async () => {
    ambientSubpageElement = await displayMainSettings(
        /*topicSource=*/ null, /*temperatureUnit=*/ null,
        /*ambientModeEnabled=*/ null);
    // Shows placeholder for ambient mode toggle row while loading ambient mode
    // status.
    const toggleRowPlaceholder =
        ambientSubpageElement.shadowRoot!.querySelector(
            '#toggleRowPlaceholder');
    assertTrue(!!toggleRowPlaceholder);

    // Preview element should show placeholders for preview images, preview
    // album info and preview album collage.
    const ambientPreview = ambientSubpageElement.shadowRoot!.querySelector(
        'ambient-preview-small');
    assertTrue(!!ambientPreview, 'ambient-preview element exists');

    // Should show image placeholders for the 3 theme items.
    const ambientThemePlaceholder =
        ambientSubpageElement.shadowRoot!.querySelector(
            '#ambientThemePlaceholder');
    assertTrue(!!ambientThemePlaceholder);

    const ambientThemeItemPlaceholders =
        ambientSubpageElement!.shadowRoot!.querySelectorAll(
            '.ambient-theme-placeholder-container:not([hidden])');
    assertEquals(3, ambientThemeItemPlaceholders!.length);

    // Should show placeholders for the 2 topic source radio buttons.
    const topicSourcePlaceholder =
        ambientSubpageElement.shadowRoot!.querySelector(
            '#topicSourcePlaceholder');
    assertTrue(!!topicSourcePlaceholder);

    const topicSourceItemPlaceholders =
        ambientSubpageElement.shadowRoot!.querySelectorAll(
            '.topic-source-placeholder:not([hidden])');
    assertEquals(2, topicSourceItemPlaceholders!.length);

    // Should show placeholders for 2 weather unit radio buttons.
    const weatherUnitPlaceholder =
        ambientSubpageElement.shadowRoot!.querySelector(
            '#weatherUnitPlaceholder');
    assertTrue(!!weatherUnitPlaceholder);

    const weatherUnitItemPlaceholders =
        ambientSubpageElement.shadowRoot!.querySelectorAll(
            '.weather-unit-placeholder:not([hidden])');
    assertEquals(2, weatherUnitItemPlaceholders!.length);

    personalizationStore.data.ambient.ambientModeEnabled = false;
    personalizationStore.data.ambient.albums = ambientProvider.albums;
    personalizationStore.data.ambient.topicSource = TopicSource.kGooglePhotos;
    personalizationStore.data.ambient.temperatureUnit =
        TemperatureUnit.kFahrenheit;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientSubpageElement);

    // Loading finished, should not show toggle row placeholder.
    assertTrue(!!toggleRowPlaceholder);
    assertEquals(getComputedStyle(toggleRowPlaceholder).display, 'none');

    // Toggle row is shown but in off status (the button is unchecked).
    const toggleRow =
        ambientSubpageElement.shadowRoot!.querySelector('toggle-row');
    assertTrue(!!toggleRow, 'toggle-row element exists');
    const toggleButton = toggleRow!.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!toggleButton, 'cr-toggle element exists');
    assertFalse(toggleButton!.checked);

    // Placeholders will be hidden for ambient theme, topic source
    // and temperature unit elements.
    assertTrue(!!ambientThemePlaceholder);
    assertEquals(
        'none', getComputedStyle(ambientThemePlaceholder).display,
        'ambient theme placeholder is hidden');

    assertTrue(!!topicSourcePlaceholder);
    assertEquals(
        'none', getComputedStyle(topicSourcePlaceholder).display,
        'topic source placeholder is hidden');

    assertTrue(!!weatherUnitPlaceholder);
    assertEquals(
        'none', getComputedStyle(weatherUnitPlaceholder).display,
        'weather unit placeholder is hidden');
  });

  test('sets ambient mode enabled in store on first load', async () => {
    personalizationStore.expectAction(
        AmbientActionName.SET_AMBIENT_MODE_ENABLED);
    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit,
        /*ambientModeEnabled=*/ false);

    await ambientProvider.whenCalled('setAmbientObserver');
    ambientProvider.updateAmbientObserver();

    const action = await personalizationStore.waitForAction(
                       AmbientActionName.SET_AMBIENT_MODE_ENABLED) as
        SetAmbientModeEnabledAction;
    assertTrue(action.enabled);
  });

  test('sets loading when there is no network', async () => {
    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit,
        /*ambientModeEnabled=*/ true);

    // Simulate going offline.
    window.dispatchEvent(new CustomEvent('offline'));
    await waitAfterNextRender(ambientSubpageElement);

    // Shows placeholder for ambient mode toggle row while loading ambient mode
    // status.
    const toggleRowPlaceholder =
        ambientSubpageElement.shadowRoot!.querySelector(
            '#toggleRowPlaceholder');
    assertTrue(!!toggleRowPlaceholder);
  });

  test('sets ambient mode when pref value changed', async () => {
    // Make sure state starts as expected.
    assertDeepEquals(emptyState(), personalizationStore.data);
    ambientSubpageElement = initElement(AmbientSubpageElement);
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
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit,
        /*ambientModeEnabled=*/ true);

    const toggleRow =
        ambientSubpageElement.shadowRoot!.querySelector('toggle-row');
    assertTrue(!!toggleRow);
    const toggleButton = toggleRow!.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!toggleButton);
    assertTrue(toggleButton!.checked);

    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(
        AmbientActionName.SET_AMBIENT_MODE_ENABLED);
    toggleButton.click();
    let action = await personalizationStore.waitForAction(
                     AmbientActionName.SET_AMBIENT_MODE_ENABLED) as
        SetAmbientModeEnabledAction;
    assertFalse(action.enabled);
    assertFalse(!!personalizationStore.data.ambient.ambientModeEnabled);
    assertFalse(toggleButton!.checked);

    personalizationStore.expectAction(
        AmbientActionName.SET_AMBIENT_MODE_ENABLED);
    toggleButton.click();
    action = await personalizationStore.waitForAction(
                 AmbientActionName.SET_AMBIENT_MODE_ENABLED) as
        SetAmbientModeEnabledAction;
    assertTrue(action.enabled);
    assertTrue(!!personalizationStore.data.ambient.ambientModeEnabled);
    assertTrue(toggleButton!.checked);
  });

  test('sets ambient mode enabled when toggle button clicked', async () => {
    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit,
        /*ambientModeEnabled=*/ true);

    const toggleRow =
        ambientSubpageElement.shadowRoot!.querySelector('toggle-row');
    assertTrue(!!toggleRow);
    const toggleButton = toggleRow!.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!toggleButton);
    assertTrue(toggleButton!.checked);

    personalizationStore.expectAction(
        AmbientActionName.SET_AMBIENT_MODE_ENABLED);
    toggleButton.click();
    let action = await personalizationStore.waitForAction(
                     AmbientActionName.SET_AMBIENT_MODE_ENABLED) as
        SetAmbientModeEnabledAction;
    assertFalse(action.enabled);

    personalizationStore.expectAction(
        AmbientActionName.SET_AMBIENT_MODE_ENABLED);
    toggleButton.click();
    action = await personalizationStore.waitForAction(
                 AmbientActionName.SET_AMBIENT_MODE_ENABLED) as
        SetAmbientModeEnabledAction;
    assertTrue(action.enabled);
  });

  test('has correct ambient theme on load', async () => {
    personalizationStore.expectAction(AmbientActionName.SET_AMBIENT_THEME);
    ambientSubpageElement = initElement(AmbientSubpageElement);

    await ambientProvider.whenCalled('setAmbientObserver');
    ambientProvider.updateAmbientObserver();

    const action =
        await personalizationStore.waitForAction(
            AmbientActionName.SET_AMBIENT_THEME) as SetAmbientThemeAction;
    assertEquals(AmbientTheme.kSlideshow, action.ambientTheme);
  });

  test('sets ambient theme when ambient theme item is clicked', async () => {
    // See "shows video ambient theme on supported devices" for expected
    // behavior when `isTimeOfDayScreenSaverEnabled` is true.
    loadTimeData.overrideValues({'isTimeOfDayScreenSaverEnabled': false});
    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit,
        /*ambientModeEnabled=*/ true);

    const ambientThemeList =
        ambientSubpageElement.shadowRoot!.querySelector('ambient-theme-list');
    assertTrue(!!ambientThemeList);
    const AmbientThemeItems = ambientThemeList!.shadowRoot!.querySelectorAll(
        'ambient-theme-item:not([hidden])');
    assertEquals(3, AmbientThemeItems!.length);
    const slideshow = AmbientThemeItems[0] as AmbientThemeItemElement;
    const feelTheBreeze = AmbientThemeItems[1] as AmbientThemeItemElement;
    assertEquals(AmbientTheme.kSlideshow, slideshow.ambientTheme);
    assertEquals(AmbientTheme.kFeelTheBreeze, feelTheBreeze.ambientTheme);

    assertEquals(feelTheBreeze.ariaChecked, 'false');
    assertEquals(slideshow.ariaChecked, 'true');

    personalizationStore.expectAction(AmbientActionName.SET_AMBIENT_THEME);
    feelTheBreeze!.click();
    let action =
        await personalizationStore.waitForAction(
            AmbientActionName.SET_AMBIENT_THEME) as SetAmbientThemeAction;
    assertEquals(AmbientTheme.kFeelTheBreeze, action.ambientTheme);

    personalizationStore.expectAction(AmbientActionName.SET_AMBIENT_THEME);
    slideshow!.click();
    action = await personalizationStore.waitForAction(
                 AmbientActionName.SET_AMBIENT_THEME) as SetAmbientThemeAction;
    assertEquals(AmbientTheme.kSlideshow, action.ambientTheme);
  });

  test('has correct topic sources on load', async () => {
    personalizationStore.expectAction(AmbientActionName.SET_TOPIC_SOURCE);
    ambientSubpageElement = initElement(AmbientSubpageElement);

    await ambientProvider.whenCalled('setAmbientObserver');
    ambientProvider.updateAmbientObserver();

    const action =
        await personalizationStore.waitForAction(
            AmbientActionName.SET_TOPIC_SOURCE) as SetTopicSourceAction;
    assertEquals(TopicSource.kArtGallery, action.topicSource);
  });

  test('sets topic source when topic source item clicked', async () => {
    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit,
        /*ambientModeEnabled=*/ true);

    const topicSourceList =
        ambientSubpageElement.shadowRoot!.querySelector('topic-source-list');
    assertTrue(!!topicSourceList);
    const topicSourceItems =
        topicSourceList!.shadowRoot!.querySelectorAll('topic-source-item');
    assertEquals(2, topicSourceItems!.length);
    const googlePhotos = topicSourceItems[0] as TopicSourceItemElement;
    const art = topicSourceItems[1] as TopicSourceItemElement;
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
    ambientSubpageElement = initElement(AmbientSubpageElement);

    await ambientProvider.whenCalled('setAmbientObserver');
    ambientProvider.updateAmbientObserver();

    const action =
        await personalizationStore.waitForAction(
            AmbientActionName.SET_TEMPERATURE_UNIT) as SetTemperatureUnitAction;
    assertEquals(TemperatureUnit.kFahrenheit, action.temperatureUnit);
  });

  test('sets temperature unit when temperature unit item clicked', async () => {
    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit,
        /*ambientModeEnabled=*/ true);

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

  test(
      'show warning in temperature section when system geolocation disabled',
      async () => {
        ambientSubpageElement = await displayMainSettings(
            TopicSource.kArtGallery, TemperatureUnit.kFahrenheit,
            /*ambientModeEnabled=*/ true);

        // Disable Privacy Hub feature flag. Geolocation content should not be
        // shown.
        loadTimeData.overrideValues({isCrosPrivacyHubLocationEnabled: false});

        const weatherUnit = ambientSubpageElement.shadowRoot!.querySelector(
            'ambient-weather-unit');
        assertTrue(!!weatherUnit);

        // Check that temperature radio buttons are shown, no matter the
        // geolocation permission value.
        for (const geolocationEnabled of [true, false]) {
          personalizationStore.data.ambient.geolocationPermissionEnabled =
              geolocationEnabled;
          personalizationStore.notifyObservers();
          await waitAfterNextRender(ambientSubpageElement);

          const temperatureUnitItems =
              weatherUnit!.shadowRoot!.querySelectorAll<CrRadioButtonElement>(
                  'cr-radio-button');
          assertEquals(2, temperatureUnitItems!.length);

          const geolocationWarningDiv =
              weatherUnit!.shadowRoot!.getElementById('geolocationWarningDiv');
          assertFalse(!!geolocationWarningDiv);
        }
        // Enable Privacy Hub feature flag.
        loadTimeData.overrideValues({isCrosPrivacyHubLocationEnabled: true});

        for (const geolocationEnabled of [true, false]) {
          personalizationStore.data.ambient.geolocationPermissionEnabled =
              geolocationEnabled;
          personalizationStore.notifyObservers();
          await waitAfterNextRender(ambientSubpageElement);

          const temperatureUnitItems =
              weatherUnit!.shadowRoot!.querySelectorAll<CrRadioButtonElement>(
                  'cr-radio-button');
          const geolocationWarningDiv =
              weatherUnit!.shadowRoot!.getElementById('geolocationWarningDiv');

          // Geolocation warning div should only be shown when the geolocation
          // permission is disabled.
          if (geolocationEnabled) {
            assertEquals(2, temperatureUnitItems!.length);
            assertFalse(!!geolocationWarningDiv);
          } else {
            assertEquals(0, temperatureUnitItems!.length);
            assertTrue(!!geolocationWarningDiv);
          }
        }
      });

  test('show Geolocation dialog and click allow', async () => {
    personalizationStore.setReducersEnabled(true);

    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit,
        /*ambientModeEnabled=*/ true);

    const weatherUnit =
        ambientSubpageElement.shadowRoot!.querySelector('ambient-weather-unit');
    assertTrue(!!weatherUnit);

    // Enable Privacy Hub feature flag.
    loadTimeData.overrideValues({isCrosPrivacyHubLocationEnabled: true});

    // Disable geolocation; this should show the warning message.
    personalizationStore.data.ambient.geolocationPermissionEnabled = false;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientSubpageElement);

    // Check warning message is present.
    let warningElement =
        weatherUnit!.shadowRoot!.getElementById('geolocationWarningDiv');
    assertTrue(!!warningElement);

    // Click the anchor to display the geolocation dialog.
    const localizedLink = warningElement.querySelector('localized-link');
    assertTrue(!!localizedLink);
    const testDetail = {event: {preventDefault: () => {}}};
    localizedLink.dispatchEvent(
        new CustomEvent('link-clicked', {bubbles: false, detail: testDetail}));
    flush();
    await waitAfterNextRender(ambientSubpageElement);

    // Check dialog has popped up.
    let geolocationDialog =
        weatherUnit.shadowRoot!.getElementById('geolocationDialog');
    assertTrue(!!geolocationDialog);
    const confirmButton =
        geolocationDialog.shadowRoot!.getElementById('confirmButton');
    assertTrue(!!confirmButton);

    // Confirm the dialog; this should enable the geolocation permission,
    // resulting in both the dialog and warning text disappearing.
    personalizationStore.expectAction(
        AmbientActionName.SET_GEOLOCATION_PERMISSION_ENABLED);
    confirmButton.click();
    const action = await personalizationStore.waitForAction(
                       AmbientActionName.SET_GEOLOCATION_PERMISSION_ENABLED) as
        SetGeolocationPermissionEnabledActionForAmbient;

    // Check the geolocation permission value has updated.
    assertTrue(action.enabled);
    assertTrue(personalizationStore.data.ambient.geolocationPermissionEnabled);

    // Check that both warning text and dialog has diappeared.
    await waitAfterNextRender(ambientSubpageElement);
    warningElement =
        weatherUnit.shadowRoot!.getElementById('geolocationWarningDiv');
    geolocationDialog =
        weatherUnit.shadowRoot!.getElementById('geolocationDialog');
    assertFalse(!!warningElement);
    assertFalse(!!geolocationDialog);
  });

  test('show Geolocation warning text when location is managed', async () => {
    // Enable Privacy Hub feature flag.
    loadTimeData.overrideValues({isCrosPrivacyHubLocationEnabled: true});

    personalizationStore.setReducersEnabled(true);

    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit,
        /*ambientModeEnabled=*/ true);

    const weatherUnit =
        ambientSubpageElement.shadowRoot!.querySelector('ambient-weather-unit');
    assertTrue(!!weatherUnit);
    // Check no warning is shown by default.
    let warningElement =
        weatherUnit!.shadowRoot!.getElementById('geolocationWarningDiv');
    assertFalse(!!warningElement);


    // Disable geolocation and mark as unmodifiable by the user. This happens
    // when the respective setting is policy-set.
    personalizationStore.data.ambient.geolocationPermissionEnabled = false;
    personalizationStore.data.ambient.geolocationIsUserModifiable = false;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientSubpageElement);

    // Check warning message is present.
    warningElement =
        weatherUnit!.shadowRoot!.getElementById('geolocationWarningDiv');
    assertTrue(!!warningElement);

    // Check that managed icon is present.
    assertTrue(!!warningElement.querySelector('cr-policy-indicator'));

    // Check that users are not prompted to change location.
    assertFalse(!!warningElement.querySelector('localized-link'));

    // Check the displayed string.
    assertEquals(
        ambientSubpageElement.i18n('geolocationWarningManagedTextForWeather'),
        warningElement.innerText);
  });

  test('duration is default to ten minutes', async () => {
    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit,
        /*ambientModeEnabled=*/ true);

    const durationElement =
        ambientSubpageElement.shadowRoot!.querySelector('ambient-duration');
    assertTrue(!!durationElement, 'Duration setting should be renderered');

    const durationOptions =
        durationElement!.shadowRoot!.querySelectorAll<HTMLOptionElement>(
            'option');
    assertEquals(
        5, durationOptions!.length, 'Duration should have exactly 5 options');

    const optionTenMin = durationOptions[1];
    assertTrue(
        optionTenMin!.selected, 'Ten minutes option is initially selected');
  });

  test('sets duration when a new duration option is selected', async () => {
    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit,
        /*ambientModeEnabled=*/ true);

    const durationElement =
        ambientSubpageElement.shadowRoot!.querySelector<HTMLSelectElement>(
            'ambient-duration');
    assertTrue(!!durationElement, 'Duration setting should be renderered');

    const durationMenu =
        durationElement!.shadowRoot!.querySelector<HTMLSelectElement>(
            '#durationOptions');
    assertTrue(!!durationMenu, 'Duration drop-down menu should be renderered');

    const durationOptions =
        durationElement!.shadowRoot!.querySelectorAll<HTMLOptionElement>(
            'option');
    const optionFiveMin = durationOptions[0];
    const optionForever = durationOptions[durationOptions.length - 1];

    personalizationStore.expectAction(
        AmbientActionName.SET_SCREEN_SAVER_DURATION);
    selectDropDownMenuOption(durationMenu, DurationOptions.FIVE_MINUTES);
    await waitAfterNextRender(ambientSubpageElement);
    assertTrue(optionFiveMin!.selected, 'Five minutes option is selected');
    let action = await personalizationStore.waitForAction(
                     AmbientActionName.SET_SCREEN_SAVER_DURATION) as
        SetScreenSaverDurationAction;
    assertEquals(5, action.minutes, 'Duration should be set to five minutes');

    personalizationStore.expectAction(
        AmbientActionName.SET_SCREEN_SAVER_DURATION);
    selectDropDownMenuOption(durationMenu, DurationOptions.FOREVER);
    await waitAfterNextRender(ambientSubpageElement);
    assertTrue(optionForever!.selected, 'Forever option is selected');
    action = await personalizationStore.waitForAction(
                 AmbientActionName.SET_SCREEN_SAVER_DURATION) as
        SetScreenSaverDurationAction;
    assertEquals(0, action.minutes, 'Duration should be set to forever');
  });

  test('has main settings visible with path ambient', async () => {
    ambientSubpageElement = initElement(
        AmbientSubpageElement, {path: Paths.AMBIENT, queryParams: {}});
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

  test('scroll to image source when clicked from thumbnail', async () => {
    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit, true,
        AmbientTheme.kSlideshow, [], 10,
        {scrollTo: ScrollableTarget.TOPIC_SOURCE_LIST});

    await ambientProvider.whenCalled('setAmbientObserver');
    ambientProvider.updateAmbientObserver();

    const imageSource =
        ambientSubpageElement.shadowRoot!.querySelector('topic-source-list');
    assertTrue(!!imageSource, 'Image source should present.');
  });

  test('has albums subpage visible with path ambient albums', async () => {
    ambientSubpageElement = initElement(AmbientSubpageElement, {
      path: Paths.AMBIENT_ALBUMS,
      queryParams: {topicSource: TopicSource.kArtGallery},
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
          PersonalizationRouterElement.reloadAtAmbient = resolve;
        });
        const albumsSubpageElement = initElement(AlbumsSubpageElement);
        personalizationStore.data.ambient.ambientModeEnabled = false;
        personalizationStore.notifyObservers();
        await waitAfterNextRender(albumsSubpageElement);

        await reloadCalledPromise;
      });

  test('show placeholders when no albums on albums subpage', async () => {
    ambientSubpageElement = initElement(AmbientSubpageElement, {
      path: Paths.AMBIENT_ALBUMS,
      queryParams: {topicSource: TopicSource.kGooglePhotos},
    });
    personalizationStore.data.ambient.ambientModeEnabled = true;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientSubpageElement);

    const albumsSubpage =
        ambientSubpageElement.shadowRoot!.querySelector('albums-subpage');
    assertTrue(!!albumsSubpage);
    assertFalse(albumsSubpage.hidden);
    await waitAfterNextRender(albumsSubpage);

    const descPlaceholder =
        albumsSubpage.shadowRoot!.querySelector('#descPlaceholderContainer');
    assertTrue(!!descPlaceholder);
    assertNotEquals(getComputedStyle(descPlaceholder).display, 'none');

    const albumsPlaceholder =
        albumsSubpage.shadowRoot!.querySelector('#albumsPlaceholderContainer');
    assertTrue(!!albumsPlaceholder);
    assertNotEquals(getComputedStyle(albumsPlaceholder).display, 'none');

    personalizationStore.data.ambient.albums = ambientProvider.albums;
    personalizationStore.data.ambient.topicSource = TopicSource.kGooglePhotos;
    personalizationStore.data.ambient.temperatureUnit =
        TemperatureUnit.kFahrenheit;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(albumsSubpage);

    assertTrue(!!descPlaceholder);
    assertEquals(getComputedStyle(descPlaceholder).display, 'none');

    assertTrue(!!albumsPlaceholder);
    assertEquals(getComputedStyle(albumsPlaceholder).display, 'none');
  });

  test('has correct albums on Google Photos albums subpage', async () => {
    ambientSubpageElement = initElement(AmbientSubpageElement, {
      path: Paths.AMBIENT_ALBUMS,
      queryParams: {topicSource: TopicSource.kGooglePhotos},
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

    const albums =
        albumList.shadowRoot!.querySelectorAll<WallpaperGridItemElement>(
            'wallpaper-grid-item:not([hidden])');
    assertEquals(1, albums.length);
  });

  test('has correct albums on Art albums subpage', async () => {
    ambientSubpageElement = initElement(AmbientSubpageElement, {
      path: Paths.AMBIENT_ALBUMS,
      queryParams: {topicSource: TopicSource.kArtGallery},
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

    const albums =
        albumList.shadowRoot!.querySelectorAll<WallpaperGridItemElement>(
            'wallpaper-grid-item:not([hidden])');
    assertEquals(3, albums.length);
    assertTrue(!!albums[0]);
    assertTrue(!!albums[1]);
    assertTrue(!!albums[2]);
  });

  test('toggle album selection by clicking', async () => {
    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(AmbientActionName.SET_ALBUMS);
    ambientSubpageElement = initElement(AmbientSubpageElement, {
      path: Paths.AMBIENT_ALBUMS,
      queryParams: {topicSource: TopicSource.kArtGallery},
    });
    personalizationStore.data.ambient.ambientModeEnabled = true;
    personalizationStore.data.ambient.albums = ambientProvider.albums;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientSubpageElement);

    await ambientProvider.whenCalled('setAmbientObserver');
    ambientProvider.updateAmbientObserver();

    const action = await personalizationStore.waitForAction(
                       AmbientActionName.SET_ALBUMS) as SetAlbumsAction;
    assertEquals(6, action.albums.length, 'action.albums.length');

    const albumsSubpage =
        ambientSubpageElement.shadowRoot!.querySelector('albums-subpage');
    assertTrue(!!albumsSubpage, '!!albumsSubpage');
    assertFalse(albumsSubpage.hidden, 'albumsSubpage.hidden');
    await waitAfterNextRender(albumsSubpage);

    const albumList = albumsSubpage.shadowRoot!.querySelector('album-list');
    assertTrue(!!albumList, '!!albumList');

    const albums =
        albumList.shadowRoot!.querySelectorAll<WallpaperGridItemElement>(
            'wallpaper-grid-item:not([hidden])');
    assertEquals(3, albums.length);
    assertTrue(!!albums[0], '!!albums[0]');
    assertTrue(!!albums[1], '!!albums[1]');
    assertTrue(!!albums[2], '!!albums[2]');
    assertFalse(albums[0].selected!, 'albums[0].selected!');
    assertFalse(albums[1].selected!, 'albums[1].selected!');
    assertTrue(albums[2].selected!, 'albums[2].selected!');
    let selectedAlbums = getSelectedAlbums(
        personalizationStore.data.ambient.albums || [],
        personalizationStore.data.ambient.topicSource!);
    assertEquals(1, selectedAlbums!.length);
    assertEquals('2', selectedAlbums[0]!.title);

    personalizationStore.expectAction(AmbientActionName.SET_ALBUM_SELECTED);
    albums[1].click();
    assertTrue(albums[1].selected!, 'albums[1].selected!');
    await personalizationStore.waitForAction(
        AmbientActionName.SET_ALBUM_SELECTED);
    selectedAlbums = getSelectedAlbums(
        personalizationStore.data.ambient.albums || [],
        personalizationStore.data.ambient.topicSource!);
    assertEquals(2, selectedAlbums!.length);
    assertEquals('1', selectedAlbums[0]!.title);
    assertEquals('2', selectedAlbums[1]!.title);
  });

  test('not deselect last art album', async () => {
    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(AmbientActionName.SET_ALBUMS);
    ambientSubpageElement = initElement(AmbientSubpageElement, {
      path: Paths.AMBIENT_ALBUMS,
      queryParams: {topicSource: TopicSource.kArtGallery},
    });
    personalizationStore.data.ambient.ambientModeEnabled = true;
    personalizationStore.data.ambient.albums = ambientProvider.albums;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientSubpageElement);

    await ambientProvider.whenCalled('setAmbientObserver');
    ambientProvider.updateAmbientObserver();

    const action = await personalizationStore.waitForAction(
                       AmbientActionName.SET_ALBUMS) as SetAlbumsAction;
    assertEquals(6, action.albums.length);

    const albumsSubpage =
        ambientSubpageElement.shadowRoot!.querySelector('albums-subpage');
    assertTrue(!!albumsSubpage);
    assertFalse(albumsSubpage.hidden);
    await waitAfterNextRender(albumsSubpage);

    const albumList = albumsSubpage.shadowRoot!.querySelector('album-list');
    assertTrue(!!albumList);

    const albums =
        albumList.shadowRoot!.querySelectorAll<WallpaperGridItemElement>(
            'wallpaper-grid-item:not([hidden])');
    assertEquals(3, albums.length);
    assertTrue(!!albums[0], '!!albums[0]');
    assertTrue(!!albums[1], '!!albums[1]');
    assertTrue(!!albums[2], '!!albums[2]');
    assertFalse(albums[0].selected!, 'albums[0].selected!');
    assertFalse(albums[1].selected!, 'albums[1].selected!');
    assertTrue(albums[2].selected!, 'albums[2].selected!');

    // Click the last art album item image will not toggle the check and will
    // show a dialog.
    albums[2].click();
    assertTrue(albums[2].selected, 'albums[2].selected');

    await waitAfterNextRender(albumsSubpage);
    const artAlbumDialog =
        albumsSubpage.shadowRoot!.querySelector('art-album-dialog');
    assertTrue(!!artAlbumDialog, '!!artAlbumDialog');
    await waitAfterNextRender(artAlbumDialog);
    assertTrue(artAlbumDialog.$.dialog.open, 'artAlbumDialog.$.dialog.open');
  });

  test('has correct album preview information', async () => {
    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(AmbientActionName.SET_ALBUMS);

    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit,
        /*ambientModeEnabled=*/ true);

    await ambientProvider.whenCalled('setAmbientObserver');
    ambientProvider.updateAmbientObserver();

    const action = await personalizationStore.waitForAction(
                       AmbientActionName.SET_ALBUMS) as SetAlbumsAction;
    assertEquals(6, action.albums.length);
    const ambientPreview = ambientSubpageElement.shadowRoot!.querySelector(
        'ambient-preview-small');
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

  test('displays zero state when ambient mode is disabled', async () => {
    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit,
        /*ambientModeEnabled=*/ false);

    const mainSettings =
        ambientSubpageElement.shadowRoot!.querySelector('#mainSettings');
    assertTrue(!!mainSettings, 'main settings should be present');

    assertEquals(
        null, mainSettings.querySelector('ambient-preview-small'),
        'preview image should be absent');

    assertEquals(
        null,
        ambientSubpageElement.shadowRoot!.querySelector('ambient-theme-list'),
        'ambient theme list should be absent');

    assertEquals(
        null,
        ambientSubpageElement.shadowRoot!.querySelector('topic-source-list'),
        'topic source list should be absent');

    assertEquals(
        null,
        ambientSubpageElement.shadowRoot!.querySelector('ambient-weather-unit'),
        'weather unit should be absent');

    const zeroState =
        ambientSubpageElement.shadowRoot!.querySelector('toggle-row');
    assertTrue(!!zeroState, 'zero state should be present');
  });

  test('displays ambient preview when ambient mode is disabled', async () => {
    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit,
        /*ambientModeEnabled=*/ false);

    const ambientPreview = ambientSubpageElement.shadowRoot!.querySelector(
        'ambient-preview-small');
    assertTrue(!!ambientPreview, 'preview image should be present');

    assertEquals(
        null,
        ambientSubpageElement.shadowRoot!.querySelector('topic-source-list'),
        'topic source list should be absent');

    assertEquals(
        null,
        ambientSubpageElement.shadowRoot!.querySelector('ambient-weather-unit'),
        'weather unit should be absent');

    assertEquals(
        null,
        ambientSubpageElement.shadowRoot!.querySelector('ambient-zero-state'),
        'zero state should not be present');
  });

  test('preview and downloading buttons should be present', async () => {
    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit,
        /*ambientModeEnabled=*/ true);

    const ambientPreview = ambientSubpageElement.shadowRoot!.querySelector(
        'ambient-preview-small');
    assertTrue(!!ambientPreview, 'ambient-preview element exists');

    const previewButton =
        ambientPreview.shadowRoot!.querySelector('.preview-button');
    assertTrue(!!previewButton, 'preview button should be present');

    personalizationStore.data.ambient.ambientUiVisibility =
        AmbientUiVisibility.kPreview;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientSubpageElement);
    const downloadingButton =
        ambientPreview.shadowRoot!.querySelector('.preview-button-disabled');
    assertTrue(!!downloadingButton, 'downloading button should be present');
  });

  test('shows video ambient theme on supported devices', async () => {
    // Enabled `isTimeOfDayScreensaverEnabled` to show the updated UI.
    loadTimeData.overrideValues({'isTimeOfDayScreenSaverEnabled': true});

    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit,
        /*ambientModeEnabled=*/ true);

    const ambientThemeList =
        ambientSubpageElement.shadowRoot!.querySelector('ambient-theme-list');
    assertTrue(!!ambientThemeList);

    const ambientThemeItems =
        ambientThemeList!.shadowRoot!.querySelectorAll<AmbientThemeItemElement>(
            'ambient-theme-item:not([hidden])');
    assertEquals(4, ambientThemeItems.length);
    const videoTheme = ambientThemeItems[3] as AmbientThemeItemElement;
    assertEquals(AmbientTheme.kVideo, videoTheme.ambientTheme);
    assertEquals('false', videoTheme.ariaChecked);

    personalizationStore.expectAction(AmbientActionName.SET_AMBIENT_THEME);
    videoTheme.click();
    const action =
        await personalizationStore.waitForAction(
            AmbientActionName.SET_AMBIENT_THEME) as SetAmbientThemeAction;
    assertEquals(AmbientTheme.kVideo, action.ambientTheme);
  });

  test('disables non-video topic sources for video animation', async () => {
    // Enabled `isTimeOfDayScreensaverEnabled` to show the updated UI.
    loadTimeData.overrideValues({'isTimeOfDayScreenSaverEnabled': true});

    ambientSubpageElement = await displayMainSettings(
        TopicSource.kArtGallery, TemperatureUnit.kFahrenheit,
        /*ambientModeEnabled=*/ true, AmbientTheme.kVideo);

    const topicSourceList =
        ambientSubpageElement.shadowRoot!.querySelector('topic-source-list');
    assertTrue(!!topicSourceList);
    const topicSourceItems =
        topicSourceList.shadowRoot!.querySelectorAll('topic-source-item');
    assertEquals(3, topicSourceItems!.length);
    const video = topicSourceItems[0] as TopicSourceItemElement;
    const googlePhotos = topicSourceItems[1] as TopicSourceItemElement;
    const art = topicSourceItems[2] as TopicSourceItemElement;
    assertEquals(TopicSource.kGooglePhotos, googlePhotos.topicSource);
    assertEquals(TopicSource.kArtGallery, art.topicSource);

    assertFalse(video.disabled);
    assertTrue(googlePhotos.disabled);
    assertTrue(art.disabled);
  });

  test('cannot deselect a video album', async () => {
    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(AmbientActionName.SET_ALBUMS);
    ambientSubpageElement = initElement(AmbientSubpageElement, {
      path: Paths.AMBIENT_ALBUMS,
      queryParams: {topicSource: TopicSource.kVideo},
    });
    personalizationStore.data.ambient.ambientModeEnabled = true;
    personalizationStore.data.ambient.albums = ambientProvider.albums;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(ambientSubpageElement);

    await ambientProvider.whenCalled('setAmbientObserver');
    ambientProvider.updateAmbientObserver();

    const action = await personalizationStore.waitForAction(
                       AmbientActionName.SET_ALBUMS) as SetAlbumsAction;
    assertEquals(6, action.albums.length);

    const albumsSubpage =
        ambientSubpageElement.shadowRoot!.querySelector('albums-subpage');
    assertTrue(!!albumsSubpage);
    assertFalse(albumsSubpage.hidden);
    await waitAfterNextRender(albumsSubpage);

    const albumList = albumsSubpage.shadowRoot!.querySelector('album-list');
    assertTrue(!!albumList);

    const albums =
        albumList.shadowRoot!.querySelectorAll<WallpaperGridItemElement>(
            'wallpaper-grid-item:not([hidden])');
    assertEquals(2, albums.length);
    assertTrue(!!albums[0]);
    assertTrue(!!albums[1]);
    assertTrue(albums[0].selected!);
    assertFalse(albums[1].selected!);

    // Attempt to de-select the selected album and expect that the album is
    // still selected.
    albums[0].click();
    assertTrue(albums[0].selected!);
  });

  test(
      'dismisses the time of day banner if ambient mode is enabled',
      async () => {
        personalizationStore.setReducersEnabled(true);
        ambientSubpageElement = await displayMainSettings(
            TopicSource.kArtGallery, TemperatureUnit.kFahrenheit,
            /*ambientModeEnabled=*/ false);

        personalizationStore.data.ambient.shouldShowTimeOfDayBanner = true;
        personalizationStore.data.ambient.ambientModeEnabled = true;
        personalizationStore.notifyObservers();
        await waitAfterNextRender(ambientSubpageElement);
        assertFalse(
            personalizationStore.data.ambient.shouldShowTimeOfDayBanner,
            'banner is dismissed');
      });
});

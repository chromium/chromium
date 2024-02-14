// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for theme-element component.  */

import 'chrome://personalization/strings.m.js';

import {emptyState, PersonalizationThemeElement, SetDarkModeEnabledAction, SetGeolocationPermissionEnabledActionForTheme, ThemeActionName, ThemeObserver} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestThemeProvider} from './test_theme_interface_provider.js';

suite('PersonalizationThemeTest', function() {
  let personalizationThemeElement: PersonalizationThemeElement|null;
  let themeProvider: TestThemeProvider;
  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    const mocks = baseSetup();
    themeProvider = mocks.themeProvider;
    personalizationStore = mocks.personalizationStore;
    ThemeObserver.initThemeObserverIfNeeded();
  });

  teardown(async () => {
    if (personalizationThemeElement) {
      personalizationThemeElement.remove();
    }
    personalizationThemeElement = null;
    ThemeObserver.shutdown();
    await flushTasks();
  });

  test('displays content', async () => {
    personalizationStore.data.theme.darkModeEnabled = false;
    personalizationStore.data.theme.colorModeAutoScheduleEnabled = false;
    personalizationThemeElement = initElement(PersonalizationThemeElement);
    await waitAfterNextRender(personalizationThemeElement);

    const radioButton =
        personalizationThemeElement.shadowRoot!.getElementById('darkMode');
    assertTrue(!!radioButton);
  });

  test('sets color mode in store on first load', async () => {
    personalizationStore.expectAction(ThemeActionName.SET_DARK_MODE_ENABLED);
    personalizationThemeElement = initElement(PersonalizationThemeElement);
    const action =
        await personalizationStore.waitForAction(
            ThemeActionName.SET_DARK_MODE_ENABLED) as SetDarkModeEnabledAction;
    assertTrue(action.enabled);
  });

  test('sets theme data in store on changed', async () => {
    // Make sure state starts as expected.
    assertDeepEquals(emptyState(), personalizationStore.data);
    await themeProvider.whenCalled('setThemeObserver');

    personalizationStore.expectAction(ThemeActionName.SET_DARK_MODE_ENABLED);
    themeProvider.themeObserverRemote!.onColorModeChanged(
        /*darkModeEnabled=*/ false);

    const {enabled} =
        await personalizationStore.waitForAction(
            ThemeActionName.SET_DARK_MODE_ENABLED) as SetDarkModeEnabledAction;
    assertFalse(enabled);
  });

  test('shows selected button on load', async () => {
    personalizationThemeElement = initElement(PersonalizationThemeElement);
    personalizationStore.data.theme.darkModeEnabled = true;
    personalizationStore.data.theme.colorModeAutoScheduleEnabled = false;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(personalizationThemeElement);
    const radioButton =
        personalizationThemeElement.shadowRoot!.getElementById('darkMode');
    assertTrue(!!radioButton);
    assertEquals(radioButton.getAttribute('aria-checked'), 'true');
  });

  test('sets dark mode enabled when dark button is clicked', async () => {
    personalizationThemeElement = initElement(PersonalizationThemeElement);
    personalizationStore.data.theme.darkModeEnabled = false;
    personalizationStore.data.theme.colorModeAutoScheduleEnabled = false;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(personalizationThemeElement);
    const radioButton =
        personalizationThemeElement.shadowRoot!.getElementById('darkMode');
    assertTrue(!!radioButton);
    assertEquals(radioButton.getAttribute('aria-checked'), 'false');

    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(ThemeActionName.SET_DARK_MODE_ENABLED);
    radioButton.click();
    const action =
        await personalizationStore.waitForAction(
            ThemeActionName.SET_DARK_MODE_ENABLED) as SetDarkModeEnabledAction;
    assertTrue(action.enabled);
    assertTrue(personalizationStore.data.theme.darkModeEnabled);
    assertEquals(radioButton.getAttribute('aria-checked'), 'true');
  });

  test('sets auto mode enabled when auto button is clicked', async () => {
    personalizationThemeElement = initElement(PersonalizationThemeElement);
    personalizationStore.data.theme.darkModeEnabled = false;
    personalizationStore.data.theme.colorModeAutoScheduleEnabled = false;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(personalizationThemeElement);
    const radioButton =
        personalizationThemeElement.shadowRoot!.getElementById('autoMode');
    assertTrue(!!radioButton);
    assertEquals(radioButton.getAttribute('aria-checked'), 'false');

    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(
        ThemeActionName.SET_COLOR_MODE_AUTO_SCHEDULE_ENABLED);
    radioButton.click();
    const action = await personalizationStore.waitForAction(
                       ThemeActionName.SET_COLOR_MODE_AUTO_SCHEDULE_ENABLED) as
        SetDarkModeEnabledAction;
    assertTrue(action.enabled);
    assertTrue(personalizationStore.data.theme.colorModeAutoScheduleEnabled);
    assertEquals(radioButton.getAttribute('aria-checked'), 'true');

    // reclicking the button does not disable auto mode.
    radioButton.click();
    assertEquals(radioButton.getAttribute('aria-checked'), 'true');
  });

  test('shows geolocation warning on location disabled', async () => {
    personalizationThemeElement = initElement(PersonalizationThemeElement);

    // Set the default sunrise/sunset time.
    personalizationStore.data.theme.sunriseTime = '6:00AM';
    personalizationStore.data.theme.sunsetTime = '6:00PM';

    // Disable Privacy Hub feature flag.
    loadTimeData.overrideValues({isCrosPrivacyHubLocationEnabled: false});

    // Check that geolocation content is not displayed on any configuration.
    for (const geolocationPermission of [true, false]) {
      for (const darkModeEnabled of [true, false]) {
        for (const autoScheduleEnabled of [true, false]) {
          personalizationStore.data.theme.geolocationPermissionEnabled =
              geolocationPermission;
          personalizationStore.data.theme.darkModeEnabled = darkModeEnabled;
          personalizationStore.data.theme.colorModeAutoScheduleEnabled =
              autoScheduleEnabled;
          personalizationStore.notifyObservers();
          await waitAfterNextRender(personalizationThemeElement);

          const warningElement =
              personalizationThemeElement.shadowRoot!.getElementById(
                  'geolocationWarningDiv');
          assertFalse(!!warningElement);
        }
      }
    }

    // Enable Privacy Hub feature flag.
    loadTimeData.overrideValues({isCrosPrivacyHubLocationEnabled: true});

    // Check that warning should not be present when geolocation permission
    // is granted.
    personalizationStore.data.theme.geolocationPermissionEnabled = true;
    // Iterate over all color mode variations.
    for (const darkModeEnabled of [true, false]) {
      for (const autoScheduleEnabled of [true, false]) {
        personalizationStore.data.theme.darkModeEnabled = darkModeEnabled;
        personalizationStore.data.theme.colorModeAutoScheduleEnabled =
            autoScheduleEnabled;
        personalizationStore.notifyObservers();
        await waitAfterNextRender(personalizationThemeElement);

        const warningElement =
            personalizationThemeElement.shadowRoot!.getElementById(
                'geolocationWarningDiv');
        assertFalse(!!warningElement);
      }
    }

    // Disable geolocation.
    personalizationStore.data.theme.geolocationPermissionEnabled = false;
    // Check that warning is only shown when Auto Schedule is selected.
    // Iterate over all color mode variations.
    for (const darkModeEnabled of [true, false]) {
      for (const autoScheduleEnabled of [true, false]) {
        personalizationStore.data.theme.darkModeEnabled = darkModeEnabled;
        personalizationStore.data.theme.colorModeAutoScheduleEnabled =
            autoScheduleEnabled;
        personalizationStore.notifyObservers();
        await waitAfterNextRender(personalizationThemeElement);

        const warningElement =
            personalizationThemeElement.shadowRoot!.getElementById(
                'geolocationWarningDiv');
        if (autoScheduleEnabled) {
          assertTrue(!!warningElement);
          const warningMessage =
              warningElement.querySelector('localized-link')?.localizedString;
          assertTrue(warningMessage?.includes('6:00AM - 6:00PM') ?? false);
        } else {
          assertFalse(!!warningElement);
        }
      }
    }
  });

  test('show Geolocation dialog and click allow', async () => {
    personalizationThemeElement = initElement(PersonalizationThemeElement);

    // Enable Privacy Hub feature flag.
    loadTimeData.overrideValues({isCrosPrivacyHubLocationEnabled: true});

    // Set the default sunrise/sunset time.
    personalizationStore.data.theme.sunriseTime = '6:00AM';
    personalizationStore.data.theme.sunsetTime = '6:00PM';

    // Disable geolocation and select Auto Schedule; This should show the
    // warning message.
    personalizationStore.data.theme.geolocationPermissionEnabled = false;
    personalizationStore.data.theme.colorModeAutoScheduleEnabled = true;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(personalizationThemeElement);

    // Check warning message is present.
    let warningElement = personalizationThemeElement.shadowRoot!.getElementById(
        'geolocationWarningDiv');
    assertTrue(!!warningElement);

    // Click the anchor to display the geolocation dialog.
    const localizedLink = warningElement.querySelector('localized-link');
    assertTrue(!!localizedLink);
    const testDetail = {event: {preventDefault: () => {}}};
    localizedLink.dispatchEvent(
        new CustomEvent('link-clicked', {bubbles: false, detail: testDetail}));
    flush();
    await waitAfterNextRender(personalizationThemeElement);

    // Check dialog has popped up.
    let geolocationDialog =
        personalizationThemeElement.shadowRoot!.getElementById(
            'geolocationDialog');
    assertTrue(!!geolocationDialog);
    const confirmButton =
        geolocationDialog.shadowRoot!.getElementById('confirmButton');
    assertTrue(!!confirmButton);

    // Confirm the dialog; this should enable the geolocation permission,
    // resulting in both the dialog and warning text disappearing.
    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(
        ThemeActionName.SET_GEOLOCATION_PERMISSION_ENABLED);
    confirmButton.click();
    const action = await personalizationStore.waitForAction(
                       ThemeActionName.SET_GEOLOCATION_PERMISSION_ENABLED) as
        SetGeolocationPermissionEnabledActionForTheme;

    // Check the geolocation permission value has updated.
    assertTrue(action.enabled);
    assertTrue(personalizationStore.data.theme.geolocationPermissionEnabled);

    // Check that both warning text and dialog has diappeared.
    await waitAfterNextRender(personalizationThemeElement);
    warningElement = personalizationThemeElement.shadowRoot!.getElementById(
        'geolocationWarningDiv');
    geolocationDialog = personalizationThemeElement.shadowRoot!.getElementById(
        'geolocationDialog');
    assertFalse(!!warningElement);
    assertFalse(!!geolocationDialog);
  });
});

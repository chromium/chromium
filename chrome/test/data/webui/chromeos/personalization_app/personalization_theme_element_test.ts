// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for theme-element component.  */

import 'chrome://personalization/strings.m.js';

import {CrTooltipIconElement, emptyState, PersonalizationThemeElement, SetDarkModeEnabledAction, setGeolocationIsUserModifiableAction, SetGeolocationIsUserModifiableActionForTheme, setGeolocationPermissionEnabledAction, SetGeolocationPermissionEnabledActionForTheme, ThemeActionName, ThemeObserver} from 'chrome://personalization/js/personalization_app.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestThemeProvider} from './test_theme_interface_provider.js';

const LIGHT_MODE_BUTTON_ID = 'lightMode';
const DARK_MODE_BUTTON_ID = 'darkMode';
const AUTO_MODE_BUTTON_ID = 'autoMode';

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

    const radioButton = personalizationThemeElement.shadowRoot!.getElementById(
        DARK_MODE_BUTTON_ID);
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
    const radioButton = personalizationThemeElement.shadowRoot!.getElementById(
        DARK_MODE_BUTTON_ID);
    assertTrue(!!radioButton);
    assertEquals(radioButton.getAttribute('aria-checked'), 'true');
  });

  test('sets dark mode enabled when dark button is clicked', async () => {
    personalizationThemeElement = initElement(PersonalizationThemeElement);
    personalizationStore.data.theme.darkModeEnabled = false;
    personalizationStore.data.theme.colorModeAutoScheduleEnabled = false;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(personalizationThemeElement);
    const radioButton = personalizationThemeElement.shadowRoot!.getElementById(
        DARK_MODE_BUTTON_ID);
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
    const radioButton = personalizationThemeElement.shadowRoot!.getElementById(
        AUTO_MODE_BUTTON_ID);
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

  async function setThemeColorMode(colorModeButtonId: string) {
    const colorModeButton: CrButtonElement =
        personalizationThemeElement!.shadowRoot!.getElementById(
            colorModeButtonId)! as CrButtonElement;
    colorModeButton.click();
    await flushTasks();
    await waitAfterNextRender(personalizationThemeElement!);
  }

  function isAutoModeLocationWarningIconPresent(): boolean {
    return !!personalizationThemeElement!.shadowRoot!.getElementById(
        'locationDeniedInfoIcon');
  }

  function isAutoModeLocationWarningIconManaged(): boolean {
    const tooltip = personalizationThemeElement!.shadowRoot!.getElementById(
                        'locationDeniedInfoIcon') as CrTooltipIconElement;
    return tooltip.iconClass! === 'personalization:managed';
  }

  // Use this helper method to set the geolocation permission in the
  // `personalizationStore`. `isManaged` is an optional parameter, used for
  // testing managed scenarios.
  async function setGeolocationPermission(
      enabled: boolean, isManaged: boolean = false) {
    personalizationStore.expectAction(
        ThemeActionName.SET_GEOLOCATION_PERMISSION_ENABLED);
    personalizationStore.dispatch(
        setGeolocationPermissionEnabledAction(enabled));
    let action: any = await personalizationStore.waitForAction(
                          ThemeActionName.SET_GEOLOCATION_PERMISSION_ENABLED) as
        SetGeolocationPermissionEnabledActionForTheme;
    assertEquals(enabled, action.enabled);

    personalizationStore.expectAction(
        ThemeActionName.SET_GEOLOCATION_IS_USER_MODIFIABLE);
    personalizationStore.dispatch(
        setGeolocationIsUserModifiableAction(!isManaged));
    action = await personalizationStore.waitForAction(
                 ThemeActionName.SET_GEOLOCATION_IS_USER_MODIFIABLE) as
        SetGeolocationIsUserModifiableActionForTheme;
    assertEquals(!isManaged, action.isUserModifiable);
  }

  test(
      'geolocation warning tooltip should be hidden when PrivacyHub disabled',
      async () => {
        // Disable Privacy Hub feature flag.
        loadTimeData.overrideValues({isCrosPrivacyHubLocationEnabled: false});

        personalizationThemeElement = initElement(PersonalizationThemeElement);
        personalizationStore.setReducersEnabled(true);

        // Check that geolocation content is not displayed on any configuration.
        await setThemeColorMode(LIGHT_MODE_BUTTON_ID);
        assertFalse(
            isAutoModeLocationWarningIconPresent(),
            'Tooltip shown when PH is disabled');
        await setThemeColorMode(DARK_MODE_BUTTON_ID);
        assertFalse(
            isAutoModeLocationWarningIconPresent(),
            'Tooltip shown when PH is disabled');
        await setThemeColorMode(AUTO_MODE_BUTTON_ID);
        assertFalse(
            isAutoModeLocationWarningIconPresent(),
            'Tooltip shown when PH is disabled');
      });


  test(
      'shows geolocation warning tooltip on location disabled by user ' +
          '(PrivacyHub enabled)',
      async () => {
        // Enable Privacy Hub feature flag.
        loadTimeData.overrideValues({isCrosPrivacyHubLocationEnabled: true});

        personalizationThemeElement = initElement(PersonalizationThemeElement);
        personalizationStore.setReducersEnabled(true);

        // Enable geolocation.
        await setGeolocationPermission(/*enabled=*/ true);
        // Check that tooltip is not shown on any configuration.
        await setThemeColorMode(LIGHT_MODE_BUTTON_ID);
        assertFalse(
            isAutoModeLocationWarningIconPresent(),
            'Tooltip shown when system geolocation is enabled');
        await setThemeColorMode(DARK_MODE_BUTTON_ID);
        assertFalse(
            isAutoModeLocationWarningIconPresent(),
            'Tooltip shown when system geolocation is enabled');
        await setThemeColorMode(AUTO_MODE_BUTTON_ID);
        assertFalse(
            isAutoModeLocationWarningIconPresent(),
            'Tooltip shown when system geolocation is enabled');

        // Disable geolocation.
        await setGeolocationPermission(/*enabled=*/ false);
        // Check that tooltip is only shown when Auto Schedule is selected.
        await setThemeColorMode(LIGHT_MODE_BUTTON_ID);
        assertFalse(
            isAutoModeLocationWarningIconPresent(),
            'Tooltip shown when AutoMode not selected');
        await setThemeColorMode(DARK_MODE_BUTTON_ID);
        assertFalse(
            isAutoModeLocationWarningIconPresent(),
            'Tooltip shown when AutoMode not selected');
        await setThemeColorMode(AUTO_MODE_BUTTON_ID);
        assertTrue(
            isAutoModeLocationWarningIconPresent(),
            'Tooltip not shown when AutoMode is selected');
        assertFalse(isAutoModeLocationWarningIconManaged());
        assertTrue(isGeolocationDialogVisible());
      });

  test(
      'show managed tooltip when location is managed by the admin',
      async () => {
        // Enable Privacy Hub feature flag.
        loadTimeData.overrideValues({isCrosPrivacyHubLocationEnabled: true});

        personalizationThemeElement = initElement(PersonalizationThemeElement);
        personalizationStore.setReducersEnabled(true);

        // Force-enable geolocation, tooltip should never be displayed.
        await setGeolocationPermission(/*enabled=*/ true, /*managed=*/ true);
        await flushTasks();
        await setThemeColorMode(LIGHT_MODE_BUTTON_ID);
        assertFalse(
            isAutoModeLocationWarningIconPresent(),
            'Tooltip shown when system geolocation is force-enabled');
        assertFalse(isGeolocationDialogVisible());
        await setThemeColorMode(DARK_MODE_BUTTON_ID);
        assertFalse(
            isAutoModeLocationWarningIconPresent(),
            'Tooltip shown when system geolocation is force-enabled');
        assertFalse(isGeolocationDialogVisible());
        await setThemeColorMode(AUTO_MODE_BUTTON_ID);
        assertFalse(
            isAutoModeLocationWarningIconPresent(),
            'Tooltip shown when system geolocation is force-enabled');
        assertFalse(isGeolocationDialogVisible());

        // Force-disable geolocation.
        await setGeolocationPermission(/*enabled=*/ false, /*managed=*/ true);
        await flushTasks();
        // Check that tooltip is only shown when Auto Schedule is selected.
        await setThemeColorMode(LIGHT_MODE_BUTTON_ID);
        assertFalse(
            isAutoModeLocationWarningIconPresent(),
            'Tooltip shown when AutoMode not selected');
        assertFalse(isGeolocationDialogVisible());
        await setThemeColorMode(DARK_MODE_BUTTON_ID);
        assertFalse(
            isAutoModeLocationWarningIconPresent(),
            'Tooltip shown when AutoMode not selected');
        assertFalse(isGeolocationDialogVisible());
        await setThemeColorMode(AUTO_MODE_BUTTON_ID);
        assertTrue(
            isAutoModeLocationWarningIconPresent(),
            'Tooltip not shown when AutoMode is selected');
        assertTrue(isAutoModeLocationWarningIconManaged(), 'wrong tooltip');
        assertFalse(isGeolocationDialogVisible());
      });

  function isGeolocationDialogVisible(): boolean {
    return !!personalizationThemeElement!.shadowRoot!.getElementById(
        'geolocationDialog');
  }

  test('show Geolocation dialog and click allow', async () => {
    // Enable Privacy Hub feature flag.
    loadTimeData.overrideValues({isCrosPrivacyHubLocationEnabled: true});

    // Load personalization theme element.
    personalizationThemeElement = initElement(PersonalizationThemeElement);

    // Set the default sunrise/sunset time.
    personalizationStore.data.theme.sunriseTime = '6:00AM';
    personalizationStore.data.theme.sunsetTime = '6:00PM';
    personalizationStore.notifyObservers();
    await flushTasks();

    personalizationStore.setReducersEnabled(true);
    // Disable geolocation and select Auto Schedule; This should show the
    // warning tooltip.
    await setGeolocationPermission(/*enabled=*/ false, /*managed=*/ false);
    await waitAfterNextRender(personalizationThemeElement);
    // personalizationStore.expectAction(
    //     ThemeActionName.SET_COLOR_MODE_AUTO_SCHEDULE_ENABLED);
    await setThemeColorMode(AUTO_MODE_BUTTON_ID);
    assertTrue(isAutoModeLocationWarningIconPresent(), 'tooltip missing');
    // Check that the dialog has popped up.
    assertTrue(isGeolocationDialogVisible(), 'dialog not being displayed');

    // Check that the dialog mentions the sunset/sunrise times.
    const geolocationDialog =
        personalizationThemeElement.shadowRoot!.getElementById(
            'geolocationDialog')!;
    const dialogBodyText =
        geolocationDialog.shadowRoot!
            .querySelector<HTMLDivElement>('#dialogBody')!.innerText;
    assertTrue(
        dialogBodyText.includes('6:00AM-6:00PM'),
        'dialog body doesn\'t include sunrise/sunset times');

    // Confirm the dialog; this should enable the geolocation permission,
    // resulting in both the dialog and warning tooltip disappearing.
    const confirmButton =
        geolocationDialog.shadowRoot!.getElementById('confirmButton');
    assertTrue(!!confirmButton);
    personalizationStore.expectAction(
        ThemeActionName.SET_GEOLOCATION_PERMISSION_ENABLED);
    confirmButton.click();
    const action = await personalizationStore.waitForAction(
                       ThemeActionName.SET_GEOLOCATION_PERMISSION_ENABLED) as
        SetGeolocationPermissionEnabledActionForTheme;
    assertTrue(action.enabled);
    // personalizationStore.notifyObservers();
    await waitAfterNextRender(personalizationThemeElement);

    // Check that both warning tooltip and dialog has diappeared.
    assertFalse(
        isAutoModeLocationWarningIconPresent(), 'tooltip didn\'t disappear');
    assertFalse(isGeolocationDialogVisible(), 'dialog didn\'t disappear');
  });

  test('Do not show geolocation dialog when location is managed', async () => {
    personalizationThemeElement = initElement(PersonalizationThemeElement);
    personalizationStore.setReducersEnabled(true);

    // Enable Privacy Hub feature flag.
    loadTimeData.overrideValues({isCrosPrivacyHubLocationEnabled: true});

    await setThemeColorMode(AUTO_MODE_BUTTON_ID);
    await setGeolocationPermission(/*enabled=*/ false, /*managed=*/ true);
    await flushTasks();

    // Check that managed icon is present, but the dialog is not triggered.
    assertTrue(isAutoModeLocationWarningIconPresent());
    assertFalse(isGeolocationDialogVisible());
  });

  test('iron-selector excludes geolocation warning', async () => {
    // Enable Privacy Hub feature flag.
    loadTimeData.overrideValues({isCrosPrivacyHubLocationEnabled: true});
    personalizationStore.data.theme.geolocationPermissionEnabled = false;
    personalizationStore.data.theme.colorModeAutoScheduleEnabled = true;

    personalizationThemeElement = initElement(PersonalizationThemeElement);
    await waitAfterNextRender(personalizationThemeElement!);

    assertEquals(
        'true',
        personalizationThemeElement.shadowRoot?.getElementById('autoMode')
            ?.ariaChecked,
        'auto mode button is checked');
    assertTrue(
        isAutoModeLocationWarningIconPresent(),
        'location warning icon is present');

    assertDeepEquals(
        ['lightMode', 'darkMode', 'autoMode'],
        personalizationThemeElement.$.selector.items?.map(item => item.id),
        'only theme buttons are selectable by iron-selector');
  });
});

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {DateTimeSettingsCardElement, SettingsDateTimePageElement, TimeZoneAutoDetectMethod, TimezoneSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {ControlledRadioButtonElement, CrSettingsPrefs, DateTimeBrowserProxy, GeolocationAccessLevel, PrefsState, Router, routes, SettingsDropdownMenuElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestDateTimeBrowserProxy} from './test_date_time_browser_proxy.js';

function getFakePrefs() {
  return {
    cros: {
      system: {
        timezone: {
          key: 'cros.system.timezone',
          type: chrome.settingsPrivate.PrefType.STRING,
          value: 'Westeros/Kings_Landing',
        },
      },
      flags: {
        // TODO(alemate): This test should be run for all possible
        // combinations of values of these options.
        per_user_timezone_enabled: {
          key: 'cros.flags.per_user_timezone_enabled',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        fine_grained_time_zone_detection_enabled: {
          key: 'cros.flags.fine_grained_time_zone_detection_enabled',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
      },
    },
    settings: {
      clock: {
        use_24hour_clock: {
          key: 'settings.clock.use_24hour_clock',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
      },
      timezone: {
        key: 'settings.timezone',
        type: chrome.settingsPrivate.PrefType.STRING,
        value: 'Westeros/Kings_Landing',
      },
    },
    generated: {
      resolve_timezone_by_geolocation_method_short: {
        key: 'generated.resolve_timezone_by_geolocation_method_short',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: TimeZoneAutoDetectMethod.IP_ONLY,
      },
      resolve_timezone_by_geolocation_on_off: {
        key: 'generated.resolve_timezone_by_geolocation_on_off',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
    },
    ash: {
      user: {
        geolocation_access_level: {
          key: 'ash.user.geolocation_access_level',
          type: GeolocationAccessLevel,
          value: GeolocationAccessLevel.ALLOWED,
        },
      },
    },
  };
}

function updatePrefsWithPolicy(
    prefs: PrefsState, managed: boolean, valueFromPolicy: boolean): Object {
  const prefsCopy = structuredClone(prefs) as any;
  if (managed) {
    prefsCopy.generated.resolve_timezone_by_geolocation_method_short
        .controlledBy = chrome.settingsPrivate.ControlledBy.USER_POLICY;
    prefsCopy.generated.resolve_timezone_by_geolocation_method_short
        .enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
    prefsCopy.generated.resolve_timezone_by_geolocation_method_short.value =
        valueFromPolicy ? TimeZoneAutoDetectMethod.IP_ONLY :
                          TimeZoneAutoDetectMethod.DISABLED;

    prefsCopy.generated.resolve_timezone_by_geolocation_on_off.controlledBy =
        chrome.settingsPrivate.ControlledBy.USER_POLICY;
    prefsCopy.generated.resolve_timezone_by_geolocation_on_off.enforcement =
        chrome.settingsPrivate.Enforcement.ENFORCED;
    prefsCopy.generated.resolve_timezone_by_geolocation_on_off.value =
        valueFromPolicy;

    prefsCopy.settings.timezone.controlledBy =
        chrome.settingsPrivate.ControlledBy.USER_POLICY;
    prefsCopy.settings.timezone.enforcement =
        chrome.settingsPrivate.Enforcement.ENFORCED;
  } else {
    prefsCopy.generated.resolve_timezone_by_geolocation_on_off.controlledBy =
        undefined;
    prefsCopy.generated.resolve_timezone_by_geolocation_on_off.enforcement =
        undefined;
    // Auto-resolve defaults to true.
    prefsCopy.generated.resolve_timezone_by_geolocation_on_off.value = true;

    prefsCopy.generated.resolve_timezone_by_geolocation_method_short
        .controlledBy = undefined;
    prefsCopy.generated.resolve_timezone_by_geolocation_method_short
        .enforcement = undefined;
    // Auto-resolve defaults to true.
    prefsCopy.generated.resolve_timezone_by_geolocation_method_short.value =
        TimeZoneAutoDetectMethod.IP_ONLY;

    prefsCopy.settings.timezone.controlledBy = undefined;
    prefsCopy.settings.timezone.enforcement = undefined;
  }
  return prefsCopy;
}

// CrOS sends time zones as [id, friendly name] pairs.
const fakeTimezones = [
  ['Westeros/Highgarden', '(KNG-2:00) The Reach Time (Highgarden)'],
  ['Westeros/Winterfell', '(KNG-1:00) The North Time (Winterfell)'],
  [
    'Westeros/Kings_Landing',
    '(KNG+0:00) Westeros Standard Time (King\'s Landing)',
  ],
  ['Westeros/TheEyrie', '(KNG+1:00) The Vale Time (The Eyrie)'],
  ['Westeros/Sunspear', '(KNG+2:00) Dorne Time (Sunspear)'],
  ['FreeCities/Pentos', '(KNG+6:00) Pentos Time (Pentos)'],
  ['FreeCities/Volantis', '(KNG+9:00) Volantis Time (Volantis)'],
  ['BayOfDragons/Daenerys', '(KNG+14:00) Daenerys Free Time (Meereen)'],
];

/**
 * Sets up fakes and creates the date time element.
 */
function initializeDateTime(
    prefs: {[key: string]: any}, hasPolicy: boolean,
    autoDetectPolicyValue?: boolean): SettingsDateTimePageElement|
    TimezoneSubpageElement {
  // Find the desired initial time zone by ID.
  const timeZone = fakeTimezones.find(
      (timeZonePair) =>
          timeZonePair[0] === prefs['cros'].system.timezone.value);

  assertTrue(!!timeZone);
  const data = {
    timeZoneID: timeZone[0],
    timeZoneName: timeZone[1],
    controlledSettingPolicy: 'This setting is managed by your administrator',
    setTimeZoneAutomaticallyDisabled: 'Automatic time zone detection disabled.',
    setTimeZoneAutomaticallyIpOnlyDefault:
        'Automatic time zone detection IP-only.',
    setTimeZoneAutomaticallyWithWiFiAccessPointsData:
        'Automatic time zone detection with WiFi AP',
    setTimeZoneAutomaticallyWithAllLocationInfo:
        'Automatic time zone detection with all location info',
    isChild: false,
  };

  loadTimeData.overrideValues(data);

  const dateTime: SettingsDateTimePageElement|TimezoneSubpageElement =
      prefs['cros'].flags.fine_grained_time_zone_detection_enabled.value ?
      document.createElement('timezone-subpage') :
      document.createElement('settings-date-time-page');
  dateTime.prefs =
      updatePrefsWithPolicy(prefs, hasPolicy, autoDetectPolicyValue || false);
  dateTime.activeTimeZoneDisplayName =
      dateTime.prefs.cros.system.timezone.value;

  CrSettingsPrefs.setInitialized();

  document.body.appendChild(dateTime);
  return dateTime;
}

suite('<settings-date-time-page>', () => {
  let dateTime: SettingsDateTimePageElement|TimezoneSubpageElement;
  let testBrowserProxy: TestDateTimeBrowserProxy;

  setup(() => {
    testBrowserProxy = new TestDateTimeBrowserProxy({fakeTimezones});
    DateTimeBrowserProxy.setInstanceForTesting(testBrowserProxy);
    CrSettingsPrefs.resetForTesting();
  });

  teardown(() => {
    dateTime.remove();
    Router.getInstance().resetRouteForTesting();
  });

  function getDateTimeSettingsCard(): DateTimeSettingsCardElement {
    const dateTimeSettingsCard =
        dateTime.shadowRoot!.querySelector('date-time-settings-card');
    assertTrue(!!dateTimeSettingsCard);
    return dateTimeSettingsCard;
  }

  function getTimeZoneAutoDetectToggle(): SettingsToggleButtonElement {
    const dateTimeSettingsCard = getDateTimeSettingsCard();
    const timeZoneAutoDetectToggle =
        dateTimeSettingsCard.shadowRoot!
            .querySelector<SettingsToggleButtonElement>(
                '#timeZoneAutoDetectToggle');
    assertTrue(!!timeZoneAutoDetectToggle);
    return timeZoneAutoDetectToggle;
  }

  function getTimeZoneAutoDetectOn(): ControlledRadioButtonElement {
    const timeZoneAutoDetectOn =
        dateTime.shadowRoot!.querySelector<ControlledRadioButtonElement>(
            '#timeZoneAutoDetectOn');
    assertTrue(!!timeZoneAutoDetectOn);
    return timeZoneAutoDetectOn;
  }

  function getTimeZoneAutoDetectOff(): ControlledRadioButtonElement {
    const timeZoneAutoDetectOff =
        dateTime.shadowRoot!.querySelector<ControlledRadioButtonElement>(
            '#timeZoneAutoDetectOff');
    assertTrue(!!timeZoneAutoDetectOff);
    return timeZoneAutoDetectOff;
  }

  function clickDisableAutoDetect(): void {
    if (dateTime.prefs.cros.flags.fine_grained_time_zone_detection_enabled
            .value) {
      getTimeZoneAutoDetectOff().click();
    } else {
      getTimeZoneAutoDetectToggle().click();
    }
  }

  function clickEnableAutoDetect(): void {
    if (dateTime.prefs.cros.flags.fine_grained_time_zone_detection_enabled
            .value) {
      getTimeZoneAutoDetectOn().click();
    } else {
      getTimeZoneAutoDetectToggle().click();
    }
  }

  function getAutodetectOnButton(): SettingsToggleButtonElement|
      ControlledRadioButtonElement {
    if (dateTime.prefs.cros.flags.fine_grained_time_zone_detection_enabled
            .value) {
      return getTimeZoneAutoDetectOn();
    }
    return getTimeZoneAutoDetectToggle();
  }

  function getTimeZoneSelector(id: string): SettingsDropdownMenuElement {
    const timezoneSelector =
        dateTime.shadowRoot!.querySelector('timezone-selector');
    assertTrue(!!timezoneSelector);
    const selectorId =
        timezoneSelector.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            id);
    assertTrue(!!selectorId);
    return selectorId;
  }

  function verifyAutoDetectSetting(
      autoDetect: boolean, managed: boolean): void {
    const selector = getTimeZoneSelector('#userTimeZoneSelector');
    assertEquals(autoDetect, selector.disabled);

    const checkButton = getAutodetectOnButton();
    const checkButtonChecked = checkButton ? checkButton.checked : false;
    if (!managed) {
      assertEquals(autoDetect, checkButtonChecked);
    }
  }

  function verifyTimeZonesPopulated(populated: boolean): void {
    const userTimezoneDropdown = getTimeZoneSelector('#userTimeZoneSelector');
    assertTrue(!!userTimezoneDropdown);
    if (populated) {
      assertEquals(
          fakeTimezones.length, userTimezoneDropdown.menuOptions.length);
    } else {
      assertEquals(1, userTimezoneDropdown.menuOptions.length);
    }
  }

  function updatePolicy(
      dateTime: SettingsDateTimePageElement|TimezoneSubpageElement,
      managed: boolean, valueFromPolicy: boolean): void {
    dateTime.prefs =
        updatePrefsWithPolicy(dateTime.prefs, managed, valueFromPolicy);
  }

  test('auto-detect on', async () => {
    const prefs = getFakePrefs();
    dateTime = initializeDateTime(prefs, false);
    flush();
    const resolveMethodDropdown =
        dateTime.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#timeZoneResolveMethodDropdown');

    assertTrue(!!resolveMethodDropdown);
    assertEquals(0, testBrowserProxy.handler.getCallCount('getTimezones'));

    verifyAutoDetectSetting(true, false);
    assertFalse(resolveMethodDropdown.disabled);
    verifyTimeZonesPopulated(false);

    clickDisableAutoDetect();
    assertEquals(1, testBrowserProxy.handler.getCallCount('getTimezones'));
    await flushTasks();

    verifyAutoDetectSetting(false, false);
    assertTrue(resolveMethodDropdown.disabled);

    verifyTimeZonesPopulated(true);
  });

  test('auto-detect off', async () => {
    dateTime = initializeDateTime(getFakePrefs(), false);
    const resolveMethodDropdown =
        dateTime.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#timeZoneResolveMethodDropdown');

    assertTrue(!!resolveMethodDropdown);
    dateTime.set(
        'prefs.generated.resolve_timezone_by_geolocation_on_off.value', false);
    dateTime.set(
        'prefs.generated.resolve_timezone_by_geolocation_method_short.value',
        TimeZoneAutoDetectMethod.DISABLED);

    assertEquals(1, testBrowserProxy.handler.getCallCount('getTimezones'));
    await flushTasks();

    verifyAutoDetectSetting(false, false);
    assertTrue(resolveMethodDropdown.disabled);
    verifyTimeZonesPopulated(true);

    clickEnableAutoDetect();

    verifyAutoDetectSetting(true, false);
    assertFalse(resolveMethodDropdown.disabled);
  });

  test('auto-detect forced on', async () => {
    const prefs = getFakePrefs();
    dateTime = initializeDateTime(prefs, true, true);
    flush();
    const resolveMethodDropdown =
        dateTime.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#timeZoneResolveMethodDropdown');

    assertTrue(!!resolveMethodDropdown);
    assertEquals(0, testBrowserProxy.handler.getCallCount('getTimezones'));

    verifyAutoDetectSetting(true, true);
    assertFalse(resolveMethodDropdown.disabled);
    verifyTimeZonesPopulated(false);

    // Cannot disable auto-detect.
    clickDisableAutoDetect();

    verifyAutoDetectSetting(true, true);
    assertFalse(resolveMethodDropdown.disabled);
    assertEquals(0, testBrowserProxy.handler.getCallCount('getTimezones'));

    // Update the policy: force auto-detect off.
    updatePolicy(dateTime, true, false);
    assertEquals(1, testBrowserProxy.handler.getCallCount('getTimezones'));
    await flushTasks();

    verifyAutoDetectSetting(false, true);
    assertTrue(resolveMethodDropdown.disabled);

    verifyTimeZonesPopulated(true);
  });

  test('auto-detect forced off', async () => {
    const prefs = getFakePrefs();
    dateTime = initializeDateTime(prefs, true, false);
    assertEquals(1, testBrowserProxy.handler.getCallCount('getTimezones'));
    await flushTasks();

    const resolveMethodDropdown =
        dateTime.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#timeZoneResolveMethodDropdown');

    assertTrue(!!resolveMethodDropdown);
    assertEquals(1, testBrowserProxy.handler.getCallCount('getTimezones'));

    verifyAutoDetectSetting(false, true);
    assertTrue(resolveMethodDropdown.disabled);
    verifyTimeZonesPopulated(true);

    // Remove the policy so user's preference takes effect.
    updatePolicy(dateTime, false, false);

    verifyAutoDetectSetting(true, false);
    assertFalse(resolveMethodDropdown.disabled);

    // User can disable auto-detect.
    clickDisableAutoDetect();

    verifyAutoDetectSetting(false, false);
  });

  test('auto-detect on supervised account', async () => {
    const prefs = getFakePrefs();
    dateTime = initializeDateTime(prefs, false);
    // Set auto detect on.
    dateTime.set(
        'prefs.generated.resolve_timezone_by_geolocation_on_off.value', true);
    dateTime.set(
        'prefs.generated.resolve_timezone_by_geolocation_method_short.value',
        TimeZoneAutoDetectMethod.IP_ONLY);

    // Set fake child account.
    loadTimeData.overrideValues({
      isChild: true,
    });

    await Router.getInstance().navigateTo(routes.DATETIME_TIMEZONE_SUBPAGE);

    const resolveMethodDropdown =
        dateTime.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#timeZoneResolveMethodDropdown');
    const timezoneSelector = getTimeZoneSelector('#userTimeZoneSelector');
    const timeZoneAutoDetectOn =
        dateTime.shadowRoot!.querySelector<ControlledRadioButtonElement>(
            '#timeZoneAutoDetectOn');
    const timeZoneAutoDetectOff =
        dateTime.shadowRoot!.querySelector<ControlledRadioButtonElement>(
            '#timeZoneAutoDetectOff');

    assertTrue(!!resolveMethodDropdown);
    assertTrue(!!timeZoneAutoDetectOn);
    assertTrue(!!timeZoneAutoDetectOff);

    // Verify elements are disabled for child account.
    assertTrue(resolveMethodDropdown.disabled);
    assertTrue(timezoneSelector.disabled);
    assertTrue(timeZoneAutoDetectOn.disabled);
    assertTrue(timeZoneAutoDetectOff.disabled);

    assertEquals(
        1,
        testBrowserProxy.handler.getCallCount('showParentAccessForTimezone'));
    testBrowserProxy.observerRemote.onParentAccessValidationComplete(
        /*success=*/ true);
    await flushTasks();

    // Verify elements are enabled.
    assertFalse(resolveMethodDropdown.disabled);
    assertFalse(timeZoneAutoDetectOn.disabled);
    assertFalse(timeZoneAutoDetectOff.disabled);

    // |timezoneSelector| is disabled when auto detect on.
    assertTrue(timezoneSelector.disabled);
  });

  test('auto-detect off supervised account', async () => {
    const prefs = getFakePrefs();
    dateTime = initializeDateTime(prefs, false);
    // Set auto detect off.
    dateTime.set(
        'prefs.generated.resolve_timezone_by_geolocation_on_off.value', false);
    dateTime.set(
        'prefs.generated.resolve_timezone_by_geolocation_method_short.value',
        TimeZoneAutoDetectMethod.DISABLED);
    // Set fake child account.
    loadTimeData.overrideValues({
      isChild: true,
    });

    await Router.getInstance().navigateTo(routes.DATETIME_TIMEZONE_SUBPAGE);

    const resolveMethodDropdown =
        dateTime.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#timeZoneResolveMethodDropdown');
    const timezoneSelector = getTimeZoneSelector('#userTimeZoneSelector');
    const timeZoneAutoDetectOn =
        dateTime.shadowRoot!.querySelector<ControlledRadioButtonElement>(
            '#timeZoneAutoDetectOn');
    const timeZoneAutoDetectOff =
        dateTime.shadowRoot!.querySelector<ControlledRadioButtonElement>(
            '#timeZoneAutoDetectOff');

    assertTrue(!!resolveMethodDropdown);
    assertTrue(!!timeZoneAutoDetectOn);
    assertTrue(!!timeZoneAutoDetectOff);

    // Verify elements are disabled for child account.
    assertTrue(resolveMethodDropdown.disabled);
    assertTrue(timezoneSelector.disabled);
    assertTrue(timeZoneAutoDetectOn.disabled);
    assertTrue(timeZoneAutoDetectOff.disabled);

    assertEquals(
        1,
        testBrowserProxy.handler.getCallCount('showParentAccessForTimezone'));
    testBrowserProxy.observerRemote.onParentAccessValidationComplete(
        /*success=*/ true);
    await flushTasks();

    // |resolveMethodDropdown| is disabled when auto detect off.
    assertTrue(resolveMethodDropdown.disabled);

    // Verify elements are enabled.
    assertFalse(timeZoneAutoDetectOn.disabled);
    assertFalse(timeZoneAutoDetectOff.disabled);
    assertFalse(timezoneSelector.disabled);
  });
});

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SettingsDateTimePageElement, TimeZoneAutoDetectMethod, TimeZoneBrowserProxyImpl, TimezoneSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {ControlledRadioButtonElement, CrSettingsPrefs, Router, routes, SettingsDropdownMenuElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util_ts.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestTimeZoneBrowserProxy} from './test_timezone_browser_proxy.js';

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
  };
}

function updatePrefsWithPolicy(
    prefs: {[key: string]: unknown}, managed: boolean,
    valueFromPolicy: boolean): Object {
  const prefsCopy = JSON.parse(JSON.stringify(prefs));
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
const fakeTimeZones = [
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
  const timeZone = fakeTimeZones.find(
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

function clickDisableAutoDetect(dateTime: SettingsDateTimePageElement|
                                TimezoneSubpageElement): void {
  if (dateTime.prefs.cros.flags.fine_grained_time_zone_detection_enabled
          .value) {
    const timeZoneAutoDetectOff =
        dateTime.shadowRoot!.querySelector<ControlledRadioButtonElement>(
            '#timeZoneAutoDetectOff');
    assertTrue(!!timeZoneAutoDetectOff);
    timeZoneAutoDetectOff.click();
  } else {
    const timeZoneAutoDetect =
        dateTime.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#timeZoneAutoDetect');
    assertTrue(!!timeZoneAutoDetect);
    timeZoneAutoDetect.click();
  }
}

function clickEnableAutoDetect(dateTime: SettingsDateTimePageElement|
                               TimezoneSubpageElement): void {
  if (dateTime.prefs.cros.flags.fine_grained_time_zone_detection_enabled
          .value) {
    const timeZoneAutoDetectOn =
        dateTime.shadowRoot!.querySelector<ControlledRadioButtonElement>(
            '#timeZoneAutoDetectOn');
    assertTrue(!!timeZoneAutoDetectOn);
    timeZoneAutoDetectOn.click();
  } else {
    const timeZoneAutoDetect =
        dateTime.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#timeZoneAutoDetect');
    assertTrue(!!timeZoneAutoDetect);
    timeZoneAutoDetect.click();
  }
}

function getAutodetectOnButton(dateTime: SettingsDateTimePageElement|
                               TimezoneSubpageElement):
    SettingsToggleButtonElement|ControlledRadioButtonElement {
  if (dateTime.prefs.cros.flags.fine_grained_time_zone_detection_enabled
          .value) {
    const timeZoneAutoDetectOn =
        dateTime.shadowRoot!.querySelector<ControlledRadioButtonElement>(
            '#timeZoneAutoDetectOn');
    assertTrue(!!timeZoneAutoDetectOn);
    return timeZoneAutoDetectOn;
  }
  const timeZoneAutoDetect =
      dateTime.shadowRoot!.querySelector<SettingsToggleButtonElement>(
          '#timeZoneAutoDetect');
  assertTrue(!!timeZoneAutoDetect);
  return timeZoneAutoDetect;
}

suite('<settings-date-time-page>', () => {
  let dateTime: SettingsDateTimePageElement|TimezoneSubpageElement;
  let testBrowserProxy: TestTimeZoneBrowserProxy;


  setup(() => {
    testBrowserProxy = new TestTimeZoneBrowserProxy();
    TimeZoneBrowserProxyImpl.setInstanceForTesting(testBrowserProxy);
    CrSettingsPrefs.resetForTesting();
  });

  teardown(() => {
    dateTime.remove();
    Router.getInstance().resetRouteForTesting();
  });

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
    const selectorHidden = selector ? selector.hidden : true;
    const selectorDisabled = selector ? selector.disabled : true;
    assertEquals(managed || autoDetect || selectorDisabled, selectorHidden);

    const checkButton = getAutodetectOnButton(dateTime);
    const checkButtonChecked = checkButton ? checkButton.checked : false;
    if (!managed) {
      assertEquals(autoDetect, checkButtonChecked);
    }
  }

  function verifyTimeZonesPopulated(populated: boolean): void {
    const userTimezoneDropdown = getTimeZoneSelector('#userTimeZoneSelector');
    const systemTimezoneDropdown =
        getTimeZoneSelector('#systemTimezoneSelector');

    const dropdown =
        userTimezoneDropdown ? userTimezoneDropdown : systemTimezoneDropdown;
    assertTrue(!!dropdown);
    if (populated) {
      assertEquals(fakeTimeZones.length, dropdown.menuOptions.length);
    } else {
      assertEquals(1, dropdown.menuOptions.length);
    }
  }

  function updatePolicy(
      dateTime: SettingsDateTimePageElement|TimezoneSubpageElement,
      managed: boolean, valueFromPolicy: boolean): void {
    dateTime.prefs =
        updatePrefsWithPolicy(dateTime.prefs, managed, valueFromPolicy);
  }

  test('auto-detect on', async () => {
    testBrowserProxy.setTimeZones(fakeTimeZones);
    const prefs = getFakePrefs();
    dateTime = initializeDateTime(prefs, false);
    flush();
    const resolveMethodDropdown =
        dateTime.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#timeZoneResolveMethodDropdown');

    assertTrue(!!resolveMethodDropdown);
    assertEquals(0, testBrowserProxy.getCallCount('getTimeZones'));

    verifyAutoDetectSetting(true, false);
    assertFalse(resolveMethodDropdown.disabled);
    verifyTimeZonesPopulated(false);

    clickDisableAutoDetect(dateTime);
    flush();

    verifyAutoDetectSetting(false, false);
    assertTrue(resolveMethodDropdown.disabled);
    await testBrowserProxy.whenCalled('getTimeZones');

    verifyTimeZonesPopulated(true);
  });

  test('auto-detect off', async () => {
    testBrowserProxy.setTimeZones(fakeTimeZones);
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

    await testBrowserProxy.whenCalled('getTimeZones');

    verifyAutoDetectSetting(false, false);
    assertTrue(resolveMethodDropdown.disabled);
    verifyTimeZonesPopulated(true);

    clickEnableAutoDetect(dateTime);

    verifyAutoDetectSetting(true, false);
    assertFalse(resolveMethodDropdown.disabled);
  });

  test('Deep link to auto set time zone on main page', async () => {
    const prefs = getFakePrefs();
    // Set fine grained time zone off so that toggle appears on this page.
    prefs.cros.flags.fine_grained_time_zone_detection_enabled.value = false;
    dateTime = initializeDateTime(prefs, false);

    const params = new URLSearchParams();
    params.append('settingId', '1001');
    Router.getInstance().navigateTo(routes.DATETIME, params);

    flush();

    const timeZoneAutoDetect =
        dateTime.shadowRoot!.querySelector('#timeZoneAutoDetect');
    assertTrue(!!timeZoneAutoDetect);
    const deepLinkElement =
        timeZoneAutoDetect.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Auto set time zone toggle should be focused for settingId=1001.');
  });

  test('auto-detect forced on', async () => {
    testBrowserProxy.setTimeZones(fakeTimeZones);
    const prefs = getFakePrefs();
    dateTime = initializeDateTime(prefs, true, true);
    flush();
    const resolveMethodDropdown =
        dateTime.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#timeZoneResolveMethodDropdown');

    assertTrue(!!resolveMethodDropdown);
    assertEquals(0, testBrowserProxy.getCallCount('getTimeZones'));

    verifyAutoDetectSetting(true, true);
    assertFalse(resolveMethodDropdown.disabled);
    verifyTimeZonesPopulated(false);

    // Cannot disable auto-detect.
    clickDisableAutoDetect(dateTime);

    verifyAutoDetectSetting(true, true);
    assertFalse(resolveMethodDropdown.disabled);
    assertEquals(0, testBrowserProxy.getCallCount('getTimeZones'));

    // Update the policy: force auto-detect off.
    updatePolicy(dateTime, true, false);

    verifyAutoDetectSetting(false, true);
    assertTrue(resolveMethodDropdown.disabled);

    await testBrowserProxy.whenCalled('getTimeZones');
    verifyTimeZonesPopulated(true);
  });

  test('auto-detect forced off', async () => {
    testBrowserProxy.setTimeZones(fakeTimeZones);
    const prefs = getFakePrefs();
    dateTime = initializeDateTime(prefs, true, false);
    const resolveMethodDropdown =
        dateTime.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#timeZoneResolveMethodDropdown');

    assertTrue(!!resolveMethodDropdown);
    await testBrowserProxy.whenCalled('getTimeZones');

    verifyAutoDetectSetting(false, true);
    assertTrue(resolveMethodDropdown.disabled);
    verifyTimeZonesPopulated(true);

    // Remove the policy so user's preference takes effect.
    updatePolicy(dateTime, false, false);

    verifyAutoDetectSetting(true, false);
    assertFalse(resolveMethodDropdown.disabled);

    // User can disable auto-detect.
    clickDisableAutoDetect(dateTime);

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

    await testBrowserProxy.whenCalled('showParentAccessForTimeZone');
    webUIListenerCallback('access-code-validation-complete');

    // Verify elements are enabled.
    assertFalse(resolveMethodDropdown.disabled);
    assertFalse(timeZoneAutoDetectOn.disabled);
    assertFalse(timeZoneAutoDetectOff.disabled);

    // |timezoneSelector| is hidden when auto detect on.
    assertFalse(timezoneSelector.disabled);
    assertTrue(timezoneSelector.hidden);
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

    await testBrowserProxy.whenCalled('showParentAccessForTimeZone');
    webUIListenerCallback('access-code-validation-complete');

    // |resolveMethodDropdown| is disabled when auto detect off.
    assertTrue(resolveMethodDropdown.disabled);

    // Verify elements are enabled.
    assertFalse(timeZoneAutoDetectOn.disabled);
    assertFalse(timeZoneAutoDetectOff.disabled);
    assertFalse(timezoneSelector.disabled);
  });

  test('set date and time button', async () => {
    const prefs = getFakePrefs();
    // Set fine grained time zone off so that toggle appears on this page.
    prefs.cros.flags.fine_grained_time_zone_detection_enabled.value = false;
    dateTime = initializeDateTime(prefs, false);

    const setDateTimeButton =
        dateTime.shadowRoot!.querySelector<CrLinkRowElement>('#setDateTime');
    assertTrue(!!setDateTimeButton);
    assertEquals(0, setDateTimeButton.offsetHeight);

    // Make the date and time editable.
    webUIListenerCallback('can-set-date-time-changed', true);
    await flushTasks();
    assertGT(setDateTimeButton.offsetHeight, 0);

    assertEquals(0, testBrowserProxy.getCallCount('showSetDateTimeUi'));
    setDateTimeButton.click();

    assertEquals(1, testBrowserProxy.getCallCount('showSetDateTimeUi'));

    // Make the date and time not editable.
    webUIListenerCallback('can-set-date-time-changed', false);
    assertEquals(0, setDateTimeButton.offsetHeight);
  });
});

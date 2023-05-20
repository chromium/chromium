// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TimeZoneAutoDetectMethod, TimeZoneBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {CrSettingsPrefs, Router, routes} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/** @implements {TimeZoneBrowserProxy} */
class TestTimeZoneBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'dateTimePageReady',
      'getTimeZones',
      'showParentAccessForTimeZone',
      'showSetDateTimeUi',
    ]);

    /** @private {!Array<!Array<string>>} */
    this.fakeTimeZones_ = [];
  }

  /** @override */
  dateTimePageReady() {
    this.methodCalled('dateTimePageReady');
  }

  /** @override */
  getTimeZones() {
    this.methodCalled('getTimeZones');
    return Promise.resolve(this.fakeTimeZones_);
  }

  /** @param {!Array<!Array<string>>} timeZones */
  setTimeZones(timeZones) {
    this.fakeTimeZones_ = timeZones;
  }

  /** @override */
  showParentAccessForTimeZone() {
    this.methodCalled('showParentAccessForTimeZone');
  }

  /** @override */
  showSetDateTimeUi() {
    this.methodCalled('showSetDateTimeUi');
  }
}

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

function updatePrefsWithPolicy(prefs, managed, valueFromPolicy) {
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

/**
 * Sets up fakes and creates the date time element.
 * @param {!Object} prefs
 * @param {boolean} hasPolicy
 * @param {boolean=} opt_autoDetectPolicyValue
 * @return {!SettingsDateTimePage}
 */
function initializeDateTime(prefs, hasPolicy, opt_autoDetectPolicyValue) {
  // Find the desired initial time zone by ID.
  const timeZone = assert(fakeTimeZones.find(function(timeZonePair) {
    return timeZonePair[0] === prefs.cros.system.timezone.value;
  }));

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

  const dateTime =
      prefs.cros.flags.fine_grained_time_zone_detection_enabled.value ?
      document.createElement('timezone-subpage') :
      document.createElement('settings-date-time-page');
  dateTime.prefs =
      updatePrefsWithPolicy(prefs, hasPolicy, opt_autoDetectPolicyValue);
  dateTime.activeTimeZoneDisplayName =
      dateTime.prefs.cros.system.timezone.value;
  CrSettingsPrefs.setInitialized();

  document.body.appendChild(dateTime);
  return dateTime;
}

function clickDisableAutoDetect(dateTime) {
  if (dateTime.prefs.cros.flags.fine_grained_time_zone_detection_enabled
          .value) {
    dateTime.shadowRoot.querySelector('#timeZoneAutoDetectOff').click();
  } else {
    dateTime.shadowRoot.querySelector('#timeZoneAutoDetect').click();
  }
}

function clickEnableAutoDetect(dateTime) {
  if (dateTime.prefs.cros.flags.fine_grained_time_zone_detection_enabled
          .value) {
    dateTime.shadowRoot.querySelector('#timeZoneAutoDetectOn').click();
  } else {
    dateTime.shadowRoot.querySelector('#timeZoneAutoDetect').click();
  }
}

function getAutodetectOnButton(dateTime) {
  if (dateTime.prefs.cros.flags.fine_grained_time_zone_detection_enabled
          .value) {
    return dateTime.shadowRoot.querySelector('#timeZoneAutoDetectOn');
  }
  return dateTime.shadowRoot.querySelector('#timeZoneAutoDetect');
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

suite('settings-date-time-page', function() {
  let dateTime;

  /** @type {?TestTimeZoneBrowserProxy} */
  let testBrowserProxy = null;


  setup(function() {
    testBrowserProxy = new TestTimeZoneBrowserProxy();
    TimeZoneBrowserProxyImpl.setInstanceForTesting(testBrowserProxy);
    PolymerTest.clearBody();
    CrSettingsPrefs.resetForTesting();
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
  });

  function getTimeZoneSelector(id) {
    return dateTime.shadowRoot.querySelector('timezone-selector')
        .shadowRoot.querySelector(id);
  }

  function verifyAutoDetectSetting(autoDetect, managed) {
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

  function verifyTimeZonesPopulated(populated) {
    const userTimezoneDropdown = getTimeZoneSelector('#userTimeZoneSelector');
    const systemTimezoneDropdown =
        getTimeZoneSelector('#systemTimezoneSelector');

    const dropdown =
        userTimezoneDropdown ? userTimezoneDropdown : systemTimezoneDropdown;
    if (populated) {
      assertEquals(fakeTimeZones.length, dropdown.menuOptions.length);
    } else {
      assertEquals(1, dropdown.menuOptions.length);
    }
  }

  function updatePolicy(dateTime, managed, valueFromPolicy) {
    dateTime.prefs =
        updatePrefsWithPolicy(dateTime.prefs, managed, valueFromPolicy);
  }

  test('auto-detect on', async function() {
    testBrowserProxy.setTimeZones(fakeTimeZones);
    const prefs = getFakePrefs();
    dateTime = initializeDateTime(prefs, false);
    flush();
    const resolveMethodDropdown =
        dateTime.shadowRoot.querySelector('#timeZoneResolveMethodDropdown');

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

  test('auto-detect off', async function() {
    testBrowserProxy.setTimeZones(fakeTimeZones);
    dateTime = initializeDateTime(getFakePrefs(), false);
    const resolveMethodDropdown =
        dateTime.shadowRoot.querySelector('#timeZoneResolveMethodDropdown');

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

    verifyAutoDetectSetting(true);
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

    const deepLinkElement =
        dateTime.shadowRoot.querySelector('#timeZoneAutoDetect')
            .shadowRoot.querySelector('cr-toggle');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Auto set time zone toggle should be focused for settingId=1001.');
  });

  test('auto-detect forced on', async function() {
    testBrowserProxy.setTimeZones(fakeTimeZones);
    const prefs = getFakePrefs();
    dateTime = initializeDateTime(prefs, true, true);
    flush();
    const resolveMethodDropdown =
        dateTime.shadowRoot.querySelector('#timeZoneResolveMethodDropdown');

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

  test('auto-detect forced off', async function() {
    testBrowserProxy.setTimeZones(fakeTimeZones);
    const prefs = getFakePrefs();
    dateTime = initializeDateTime(prefs, true, false);
    const resolveMethodDropdown =
        dateTime.shadowRoot.querySelector('#timeZoneResolveMethodDropdown');

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
        dateTime.shadowRoot.querySelector('#timeZoneResolveMethodDropdown');
    const timezoneSelector = getTimeZoneSelector('#userTimeZoneSelector');
    const timeZoneAutoDetectOn =
        dateTime.shadowRoot.querySelector('#timeZoneAutoDetectOn');
    const timeZoneAutoDetectOff =
        dateTime.shadowRoot.querySelector('#timeZoneAutoDetectOff');

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
        dateTime.shadowRoot.querySelector('#timeZoneResolveMethodDropdown');
    const timezoneSelector = getTimeZoneSelector('#userTimeZoneSelector');
    const timeZoneAutoDetectOn =
        dateTime.shadowRoot.querySelector('#timeZoneAutoDetectOn');
    const timeZoneAutoDetectOff =
        dateTime.shadowRoot.querySelector('#timeZoneAutoDetectOff');

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

  test('set date and time button', async function() {
    const prefs = getFakePrefs();
    // Set fine grained time zone off so that toggle appears on this page.
    prefs.cros.flags.fine_grained_time_zone_detection_enabled.value = false;
    dateTime = initializeDateTime(prefs, false);

    const setDateTimeButton = dateTime.shadowRoot.querySelector('#setDateTime');
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
    assertEquals(setDateTimeButton.offsetHeight, 0);
  });
});

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
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
        value: settings.TimeZoneAutoDetectMethod.IP_ONLY,
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
        valueFromPolicy ? settings.TimeZoneAutoDetectMethod.IP_ONLY :
                          settings.TimeZoneAutoDetectMethod.DISABLED;

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
        settings.TimeZoneAutoDetectMethod.IP_ONLY;

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
    return timeZonePair[0] == prefs.cros.system.timezone.value;
  }));

  const data = {
    timeZoneID: timeZone[0],
    timeZoneName: timeZone[1],
    controlledSettingPolicy: 'This setting is enforced by your administrator',
    setTimeZoneAutomaticallyDisabled: 'Automatic time zone detection disabled.',
    setTimeZoneAutomaticallyIpOnlyDefault:
        'Automatic time zone detection IP-only.',
    setTimeZoneAutomaticallyWithWiFiAccessPointsData:
        'Automatic time zone detection with WiFi AP',
    setTimeZoneAutomaticallyWithAllLocationInfo:
        'Automatic time zone detection with all location info',
  };

  window.loadTimeData = new LoadTimeData;
  loadTimeData.data = data;

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
    dateTime.$$('#timeZoneAutoDetectOff').click();
  } else {
    dateTime.$$('#timeZoneAutoDetect').click();
  }
}

function clickEnableAutoDetect(dateTime) {
  if (dateTime.prefs.cros.flags.fine_grained_time_zone_detection_enabled
          .value) {
    dateTime.$$('#timeZoneAutoDetectOn').click();
  } else {
    dateTime.$$('#timeZoneAutoDetect').click();
  }
}

function getAutodetectOnButton(dateTime) {
  if (dateTime.prefs.cros.flags.fine_grained_time_zone_detection_enabled
          .value) {
    return dateTime.$$('#timeZoneAutoDetectOn');
  }
  return dateTime.$$('#timeZoneAutoDetect');
}

// CrOS sends time zones as [id, friendly name] pairs.
const fakeTimeZones = [
  ['Westeros/Highgarden', '(KNG-2:00) The Reach Time (Highgarden)'],
  ['Westeros/Winterfell', '(KNG-1:00) The North Time (Winterfell)'],
  [
    'Westeros/Kings_Landing',
    '(KNG+0:00) Westeros Standard Time (King\'s Landing)'
  ],
  ['Westeros/TheEyrie', '(KNG+1:00) The Vale Time (The Eyrie)'],
  ['Westeros/Sunspear', '(KNG+2:00) Dorne Time (Sunspear)'],
  ['FreeCities/Pentos', '(KNG+6:00) Pentos Time (Pentos)'],
  ['FreeCities/Volantis', '(KNG+9:00) Volantis Time (Volantis)'],
  ['BayOfDragons/Daenerys', '(KNG+14:00) Daenerys Free Time (Meereen)'],
];

suite('settings-date-time-page', function() {
  let dateTime;

  // Track whether handler functions have been called.
  let dateTimePageReadyCalled;
  let getTimeZonesCalled;

  setup(function() {
    PolymerTest.clearBody();
    CrSettingsPrefs.resetForTesting();

    dateTimePageReadyCalled = false;
    getTimeZonesCalled = false;

    registerMessageCallback('dateTimePageReady', null, function() {
      assertFalse(dateTimePageReadyCalled);
      dateTimePageReadyCalled = true;
    });

    registerMessageCallback('getTimeZones', null, function(callback) {
      assertFalse(getTimeZonesCalled);
      getTimeZonesCalled = true;
      cr.webUIResponse(callback, true, fakeTimeZones);
    });
  });

  suiteTeardown(function() {
    // TODO(michaelpg): Removes the element before exiting, because the
    // <paper-tooltip> in <cr-policy-indicator> somehow causes warnings
    // and/or script errors in axs_testing.js.
    PolymerTest.clearBody();
  });

  function checkDateTimePageReadyCalled() {
    if (dateTime.prefs.cros.flags.fine_grained_time_zone_detection_enabled
            .value) {
      return;
    }
    assertTrue(dateTimePageReadyCalled);
  }

  function getTimeZoneSelector(id) {
    return dateTime.$$('timezone-selector').$$(id);
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

  test('auto-detect on', function(done) {
    const prefs = getFakePrefs();
    dateTime = initializeDateTime(prefs, false);
    const resolveMethodDropdown = dateTime.$$('#timeZoneResolveMethodDropdown');

    setTimeout(function() {
      checkDateTimePageReadyCalled();
      assertFalse(getTimeZonesCalled);

      verifyAutoDetectSetting(true, false);
      assertFalse(resolveMethodDropdown.disabled);
      verifyTimeZonesPopulated(false);

      clickDisableAutoDetect(dateTime);
      Polymer.dom.flush();
    });

    setTimeout(function() {
      verifyAutoDetectSetting(false, false);
      assertTrue(resolveMethodDropdown.disabled);
      assertTrue(getTimeZonesCalled);

      verifyTimeZonesPopulated(true);
      done();
    });
  });

  test('auto-detect off', function(done) {
    dateTime = initializeDateTime(getFakePrefs(), false);
    const resolveMethodDropdown = dateTime.$$('#timeZoneResolveMethodDropdown');

    setTimeout(function() {
      dateTime.set(
          'prefs.generated.resolve_timezone_by_geolocation_on_off.value',
          false);
      dateTime.set(
          'prefs.generated.resolve_timezone_by_geolocation_method_short.value',
          settings.TimeZoneAutoDetectMethod.DISABLED);
    });

    setTimeout(function() {
      checkDateTimePageReadyCalled();
      assertTrue(getTimeZonesCalled);

      verifyAutoDetectSetting(false, false);
      assertTrue(resolveMethodDropdown.disabled);
      verifyTimeZonesPopulated(true);

      clickEnableAutoDetect(dateTime);
    });

    setTimeout(function() {
      verifyAutoDetectSetting(true);
      assertFalse(resolveMethodDropdown.disabled);
      done();
    });
  });

  test('auto-detect forced on', function(done) {
    const prefs = getFakePrefs();
    dateTime = initializeDateTime(prefs, true, true);
    const resolveMethodDropdown = dateTime.$$('#timeZoneResolveMethodDropdown');

    setTimeout(function() {
      checkDateTimePageReadyCalled();
      assertFalse(getTimeZonesCalled);

      verifyAutoDetectSetting(true, true);
      assertFalse(resolveMethodDropdown.disabled);
      verifyTimeZonesPopulated(false);

      // Cannot disable auto-detect.
      clickDisableAutoDetect(dateTime);
    });

    setTimeout(function() {
      verifyAutoDetectSetting(true, true);
      assertFalse(resolveMethodDropdown.disabled);
      assertFalse(getTimeZonesCalled);

      // Update the policy: force auto-detect off.
      updatePolicy(dateTime, true, false);
    });
    setTimeout(function() {
      verifyAutoDetectSetting(false, true);
      assertTrue(resolveMethodDropdown.disabled);

      assertTrue(getTimeZonesCalled);
      verifyTimeZonesPopulated(true);
      done();
    });
  });

  test('auto-detect forced off', function(done) {
    const prefs = getFakePrefs();
    dateTime = initializeDateTime(prefs, true, false);
    const resolveMethodDropdown = dateTime.$$('#timeZoneResolveMethodDropdown');

    setTimeout(function() {
      checkDateTimePageReadyCalled();
      assertTrue(getTimeZonesCalled);

      verifyAutoDetectSetting(false, true);
      assertTrue(resolveMethodDropdown.disabled);
      verifyTimeZonesPopulated(true);

      // Remove the policy so user's preference takes effect.
      updatePolicy(dateTime, false, false);
    });

    setTimeout(function() {
      verifyAutoDetectSetting(true, false);
      assertFalse(resolveMethodDropdown.disabled);

      // User can disable auto-detect.
      clickDisableAutoDetect(dateTime);
    });
    setTimeout(function() {
      verifyAutoDetectSetting(false, false);
      done();
    });
  });

  test('set date and time button', function() {
    dateTime = initializeDateTime(getFakePrefs(), false);

    let showSetDateTimeUICalled = false;
    registerMessageCallback('showSetDateTimeUI', null, function() {
      assertFalse(showSetDateTimeUICalled);
      showSetDateTimeUICalled = true;
    });

    setTimeout(function() {
      const setDateTimeButton = dateTime.$$('#setDateTime');
      assertEquals(0, setDateTimeButton.offsetHeight);

      // Make the date and time editable.
      cr.webUIListenerCallback('can-set-date-time-changed', true);
      assertGT(setDateTimeButton.offsetHeight, 0);

      assertFalse(showSetDateTimeUICalled);
      setDateTimeButton.click();
    });
    setTimeout(function() {
      assertTrue(showSetDateTimeUICalled);

      // Make the date and time not editable.
      cr.webUIListenerCallback('can-set-date-time-changed', false);
      assertEquals(setDateTimeButton.offsetHeight, 0);
    });
  });
});
})();

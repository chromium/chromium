// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {DateTimeSettingsCardElement, TimeZoneAutoDetectMethod, TimezoneSelectorElement} from 'chrome://os-settings/lazy_load.js';
import {CrLinkRowElement, CrSettingsPrefs, DateTimeBrowserProxy, PrefsState, Router, routes, settingMojom, SettingsDropdownMenuElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestDateTimeBrowserProxy} from './test_date_time_browser_proxy.js';

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

suite('<date-time-settings-card>', () => {
  const isRevampWayfindingEnabled =
      loadTimeData.getBoolean('isRevampWayfindingEnabled');
  const route =
      isRevampWayfindingEnabled ? routes.SYSTEM_PREFERENCES : routes.DATETIME;

  let dateTimeSettingsCard: DateTimeSettingsCardElement;
  let testBrowserProxy: TestDateTimeBrowserProxy;

  setup(() => {
    testBrowserProxy = new TestDateTimeBrowserProxy({fakeTimezones});
    DateTimeBrowserProxy.setInstanceForTesting(testBrowserProxy);
    CrSettingsPrefs.resetForTesting();
  });

  teardown(() => {
    dateTimeSettingsCard.remove();
    Router.getInstance().resetRouteForTesting();
  });

  function createDateTimeSettingsCard(prefs: PrefsState): void {
    const initialTimezoneDisplayName = prefs['cros'].system.timezone.value;

    // Find the desired initial timezone by ID.
    const timeZone = fakeTimezones.find(
        (timeZonePair) => timeZonePair[0] === initialTimezoneDisplayName);
    assertTrue(!!timeZone);
    const [timeZoneID, timeZoneName] = timeZone;
    loadTimeData.overrideValues({
      timeZoneID,
      timeZoneName,
    });

    dateTimeSettingsCard = document.createElement('date-time-settings-card');
    dateTimeSettingsCard.prefs = prefs;
    dateTimeSettingsCard.activeTimeZoneDisplayName = initialTimezoneDisplayName;
    CrSettingsPrefs.setInitialized();
    document.body.appendChild(dateTimeSettingsCard);
    flush();
  }

  function getTimezoneAutoDetectToggle(): SettingsToggleButtonElement {
    const element = dateTimeSettingsCard.shadowRoot!
                        .querySelector<SettingsToggleButtonElement>(
                            '#timeZoneAutoDetectToggle');
    assertTrue(!!element);
    return element;
  }

  function getTimeZoneSelector(): TimezoneSelectorElement {
    const timezoneSelector =
        dateTimeSettingsCard.shadowRoot!.querySelector('timezone-selector');
    assertTrue(!!timezoneSelector);
    return timezoneSelector;
  }

  function getUserTimezoneDropdown(): SettingsDropdownMenuElement {
    const timezoneSelector = getTimeZoneSelector();
    const userTimezoneDropdown =
        timezoneSelector.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#userTimeZoneSelector');
    assertTrue(!!userTimezoneDropdown);
    return userTimezoneDropdown;
  }

  function assertTimezonesPopulated(isPopulated: boolean): void {
    const timezoneDropdown = getUserTimezoneDropdown();
    const numExpected = isPopulated ? fakeTimezones.length : 1;
    assertEquals(numExpected, timezoneDropdown.menuOptions.length);
  }

  test('Set date and time row only shows if editable', async () => {
    const prefs = getFakePrefs();
    createDateTimeSettingsCard(prefs);

    const setDateTimeRow =
        dateTimeSettingsCard.shadowRoot!.querySelector<CrLinkRowElement>(
            '#setDateTimeRow');
    assertFalse(isVisible(setDateTimeRow));

    // Make the date and time editable.
    testBrowserProxy.observerRemote.onSystemClockCanSetTimeChanged(
        /*is_allowed=*/ true);
    await flushTasks();
    assertTrue(isVisible(setDateTimeRow));

    assertEquals(0, testBrowserProxy.handler.getCallCount('showSetDateTimeUI'));
    setDateTimeRow!.click();
    assertEquals(1, testBrowserProxy.handler.getCallCount('showSetDateTimeUI'));

    // Make the date and time not editable.
    testBrowserProxy.observerRemote.onSystemClockCanSetTimeChanged(
        /*is_allowed=*/ false);
    await flushTasks();
    assertFalse(isVisible(setDateTimeRow));
  });

  suite('When timezone detection is enabled', () => {
    setup(() => {
      const prefs = getFakePrefs();
      prefs.cros.flags.fine_grained_time_zone_detection_enabled.value = true;
      createDateTimeSettingsCard(prefs);
    });

    test('Timezone selector is hidden', () => {
      const timezoneSelector =
          dateTimeSettingsCard.shadowRoot!.querySelector('timezone-selector');
      assertFalse(isVisible(timezoneSelector));
    });

    test('Timezone auto-detect toggle is hidden', () => {
      const toggle = dateTimeSettingsCard.shadowRoot!
                         .querySelector<SettingsToggleButtonElement>(
                             '#timeZoneAutoDetectToggle');
      assertFalse(isVisible(toggle));
    });

    test('Timezone subpage row is visible', () => {
      const rowElement =
          dateTimeSettingsCard.shadowRoot!.querySelector<HTMLElement>(
              '#timeZoneSettingsTrigger');
      assertTrue(isVisible(rowElement));
    });

    test(
        'Timezone subpage row is focused after returning from subpage',
        async () => {
          const triggerSelector = '#timeZoneSettingsTrigger';
          const rowElement =
              dateTimeSettingsCard.shadowRoot!.querySelector<HTMLElement>(
                  triggerSelector);
          assertTrue(!!rowElement);
          rowElement.click();
          flush();

          const popStateEventPromise = eventToPromise('popstate', window);
          Router.getInstance().navigateToPreviousRoute();
          await popStateEventPromise;
          await waitAfterNextRender(dateTimeSettingsCard);

          assertEquals(
              rowElement, dateTimeSettingsCard.shadowRoot!.activeElement,
              `${triggerSelector} should be focused.`);
        });
  });

  suite('When timezone detection is disabled', () => {
    setup(() => {
      const prefs = getFakePrefs();
      prefs.cros.flags.fine_grained_time_zone_detection_enabled.value = false;
      createDateTimeSettingsCard(prefs);
    });

    test('Timezone selector is visible', () => {
      const timezoneSelector =
          dateTimeSettingsCard.shadowRoot!.querySelector('timezone-selector');
      assertTrue(isVisible(timezoneSelector));
    });

    test('Timezone auto-detect toggle is visible', () => {
      const toggle = getTimezoneAutoDetectToggle();
      assertTrue(isVisible(toggle));
    });

    test('Timezone detect toggle is deep linkable', async () => {
      const params = new URLSearchParams();
      const settingId = settingMojom.Setting.kChangeTimeZone.toString();
      params.append('settingId', settingId);
      Router.getInstance().navigateTo(route, params);
      flush();

      const toggle = getTimezoneAutoDetectToggle();
      await waitAfterNextRender(toggle);
      assertEquals(
          toggle, dateTimeSettingsCard.shadowRoot!.activeElement,
          `Auto set time zone toggle should be focused for settingId=${
              settingId}.`);
    });

    test('Turning off timezone auto-detection loads timezones', async () => {
      assertEquals(0, testBrowserProxy.handler.getCallCount('getTimezones'));

      const autodetectToggle = getTimezoneAutoDetectToggle();
      assertTrue(
          autodetectToggle.checked, 'Expected auto-detect toggle to be on');
      assertTimezonesPopulated(false);

      autodetectToggle.click();
      assertFalse(
          autodetectToggle.checked, 'Expected auto-detect toggle to be off');
      assertEquals(1, testBrowserProxy.handler.getCallCount('getTimezones'));
      await flushTasks();

      assertTimezonesPopulated(true);
    });

    test('Auto-detect toggle controls pref', () => {
      const autodetectToggle = getTimezoneAutoDetectToggle();
      assertTrue(
          autodetectToggle.checked, 'Expected auto-detect toggle to be on');
      assertTrue(dateTimeSettingsCard.get(
          'prefs.generated.resolve_timezone_by_geolocation_on_off.value'));

      autodetectToggle.click();
      assertFalse(
          autodetectToggle.checked, 'Expected auto-detect toggle to be off');
      assertFalse(dateTimeSettingsCard.get(
          'prefs.generated.resolve_timezone_by_geolocation_on_off.value'));
    });
  });
});

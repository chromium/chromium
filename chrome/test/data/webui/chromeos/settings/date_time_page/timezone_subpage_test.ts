// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsRadioGroupElement, TimezoneSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {CrSettingsPrefs, GeolocationAccessLevel, Router, routes} from 'chrome://os-settings/os_settings.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

let timezoneSubpage: TimezoneSubpageElement;

async function init(): Promise<void> {
  const prefElement = document.createElement('settings-prefs');
  document.body.appendChild(prefElement);

  await CrSettingsPrefs.initialized;
  timezoneSubpage = document.createElement('timezone-subpage');
  timezoneSubpage.prefs = {
    ...prefElement.prefs,
    ash: {
      user: {
        geolocation_access_level: {
          key: 'ash.user.geolocation_access_level',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: GeolocationAccessLevel.ALLOWED,
        },
      },
    },
  };

  document.body.appendChild(timezoneSubpage);
  await flushTasks();
}

function testTeardown() {
  timezoneSubpage.remove();
  CrSettingsPrefs.resetForTesting();
  Router.getInstance().resetRouteForTesting();
}

suite('<timezone-subpage> with logged-in user', () => {
  setup(async () => {
    loadTimeData.overrideValues({canSetSystemTimezone: true});
    await init();
  });

  teardown(() => {
    testTeardown();
  });

  test('timezone radio group is enabled', async () => {
    // Enable automatic timezone.
    timezoneSubpage.setPrefValue(
        'generated.resolve_timezone_by_geolocation_on_off', true);
    await flushTasks();

    const timezoneRadioGroup =
        timezoneSubpage.shadowRoot!.querySelector<SettingsRadioGroupElement>(
            '#timeZoneRadioGroup');
    assert(timezoneRadioGroup);
    assertFalse(timezoneRadioGroup.disabled);
  });

  test('Timezone autodetect by geolocation radio', async () => {
    const timezoneRadioGroup =
        timezoneSubpage.shadowRoot!.querySelector<SettingsRadioGroupElement>(
            '#timeZoneRadioGroup');
    assert(timezoneRadioGroup);

    // Resolve timezone by geolocation is on.
    timezoneSubpage.setPrefValue(
        'generated.resolve_timezone_by_geolocation_on_off', true);
    await flushTasks();
    assertEquals('true', timezoneRadioGroup.selected);

    // Resolve timezone by geolocation is off.
    timezoneSubpage.setPrefValue(
        'generated.resolve_timezone_by_geolocation_on_off', false);
    await flushTasks();
    assertEquals('false', timezoneRadioGroup.selected);

    // Set timezone autodetect on by clicking the 'on' radio.
    const timezoneAutodetectOn =
        timezoneSubpage.shadowRoot!.querySelector<HTMLElement>(
            '#timeZoneAutoDetectOn');
    assert(timezoneAutodetectOn);
    timezoneAutodetectOn.click();
    assertTrue(timezoneSubpage
                   .getPref('generated.resolve_timezone_by_geolocation_on_off')
                   .value);

    // Turn timezone autodetect off by clicking the 'off' radio.
    const timezoneAutodetectOff =
        timezoneSubpage.shadowRoot!.querySelector<HTMLElement>(
            '#timeZoneAutoDetectOff');
    assert(timezoneAutodetectOff);
    timezoneAutodetectOff.click();
    assertFalse(timezoneSubpage
                    .getPref('generated.resolve_timezone_by_geolocation_on_off')
                    .value);
  });

  test('Deep link to time zone setter on subpage', async () => {
    // Resolve timezone by geolocation is on.
    timezoneSubpage.setPrefValue(
        'generated.resolve_timezone_by_geolocation_on_off', true);

    const params = new URLSearchParams();
    params.append('settingId', '1001');
    Router.getInstance().navigateTo(routes.DATETIME_TIMEZONE_SUBPAGE, params);

    const deepLinkElement =
        timezoneSubpage.shadowRoot!.querySelector('#timeZoneAutoDetectOn')!
            .shadowRoot!.querySelector<HTMLElement>('#button');
    assert(deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Auto set time zone toggle should be focused for settingId=1001.');
  });

  test(
      'automatic timezone shows geolocation warning when location is disabled',
      async () => {
        const timezoneRadioGroup =
            timezoneSubpage.shadowRoot!
                .querySelector<SettingsRadioGroupElement>(
                    '#timeZoneRadioGroup');
        assert(timezoneRadioGroup);

        // Enable automatic timezone.
        timezoneSubpage.setPrefValue(
            'generated.resolve_timezone_by_geolocation_on_off', true);

        // Geolocation is allowed by default, the warning text should be hidden.
        assertFalse(isVisible(
            timezoneSubpage.shadowRoot!.querySelector('#warningText')));

        // Disable geolocation permission and check warning is shown.
        timezoneSubpage.setPrefValue(
            'ash.user.geolocation_access_level',
            GeolocationAccessLevel.DISALLOWED);
        await flushTasks();

        const warningText =
            timezoneSubpage.shadowRoot!.querySelector('#warningText');
        assertTrue(!!warningText);
        // Check that warning contains the link anchor.
        assertTrue(
            warningText?.getAttribute('warning-text-with-anchor')
                ?.includes('<a href="#">') ??
            false);
      });

  test(
      'time zone shows warning w/o hyperlink when location is managed',
      async () => {
        const timezoneRadioGroup =
            timezoneSubpage.shadowRoot!
                .querySelector<SettingsRadioGroupElement>(
                    '#timeZoneRadioGroup');
        assert(timezoneRadioGroup);

        // Enable automatic timezone.
        timezoneSubpage.setPrefValue(
            'generated.resolve_timezone_by_geolocation_on_off', true);
        // Geolocation is allowed by default, the warning text should be hidden.
        assertFalse(isVisible(
            timezoneSubpage.shadowRoot!.querySelector('#warningText')));

        // Set the geolocation pref to forced off, to replicate the policy
        // value.
        timezoneSubpage.setPrefValue(
            'ash.user.geolocation_access_level',
            GeolocationAccessLevel.DISALLOWED);
        timezoneSubpage.prefs.ash.user.geolocation_access_level.enforcement =
            chrome.settingsPrivate.Enforcement.ENFORCED;
        timezoneSubpage.notifyPath(
            'prefs.ash.user.geolocation_access_level.enforcement',
            chrome.settingsPrivate.Enforcement.ENFORCED);
        await flushTasks();

        const warningText =
            timezoneSubpage.shadowRoot!.querySelector('#warningText');
        assertTrue(!!warningText);
        // Check that the warning doesn't have clickable section to launch the
        // dialog.
        assertFalse(
            warningText?.getAttribute('warning-text-with-anchor')
                ?.includes('<a href="#">') ??
            false);
      });
});

suite('<timezone-subpage> with user who can not set system timezone', () => {
  setup(async () => {
    loadTimeData.overrideValues({canSetSystemTimezone: false});
    await init();
  });

  teardown(() => {
    testTeardown();
  });

  test('timezone radio group is disabled', async () => {
    // Enable automatic timezone.
    timezoneSubpage.setPrefValue(
        'generated.resolve_timezone_by_geolocation_on_off', true);
    await flushTasks();

    const timezoneRadioGroup =
        timezoneSubpage.shadowRoot!.querySelector<SettingsRadioGroupElement>(
            '#timeZoneRadioGroup');
    assert(timezoneRadioGroup);
    assertTrue(timezoneRadioGroup.disabled);
  });
});

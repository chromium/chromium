// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsClearBrowsingDataTimePicker} from 'chrome://settings/lazy_load.js';
import {getTimePeriodString, TimePeriod} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

// clang-format on

suite('DeleteBrowsingDataTimePicker', function() {
  let timePicker: SettingsClearBrowsingDataTimePicker;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    timePicker =
        document.createElement('settings-clear-browsing-data-time-picker');
    timePicker.prefs = settingsPrefs.prefs;
    timePicker.setPrefValue(
        'browser.clear_data.time_period', TimePeriod.LAST_HOUR);

    document.body.appendChild(timePicker);
    return flushTasks();
  });

  function getChipForTimePeriod(timePeriod: TimePeriod): HTMLElement|undefined {
    const visibleOptions =
        timePicker.shadowRoot!.querySelectorAll<HTMLElement>('cr-chip');
    assertTrue(!!visibleOptions);

    for (const option of visibleOptions) {
      if (option.textContent!.trim() === getTimePeriodString(timePeriod)) {
        return option;
      }
    }
    return undefined;
  }

  function getMenuItemForTimePeriod(timePeriod: TimePeriod): HTMLElement|
      undefined {
    // Open the 'More' dropdown menu.
    timePicker.$.moreButton.click();
    flush();

    const menuItems =
        timePicker.shadowRoot!.querySelectorAll<HTMLElement>('.dropdown-item');
    assertTrue(!!menuItems);

    for (const item of menuItems) {
      if (item.textContent!.trim() === getTimePeriodString(timePeriod)) {
        return item;
      }
    }
    return undefined;
  }

  function getSelectedChip(): HTMLElement|undefined {
    const selectedChips = timePicker.shadowRoot!.querySelectorAll<HTMLElement>(
        'cr-chip[selected]');
    assertTrue(!!selectedChips);

    // Verify there is only one selected chip.
    assertEquals(selectedChips.length, 1);
    assertTrue(!!selectedChips[0]);

    // Verify the check icon is visible on the selected element.
    assertTrue(isVisible(selectedChips[0].querySelector('cr-icon')));
    return selectedChips[0];
  }

  function verifyChipsExistForTimePeriods(timePeriods: TimePeriod[]) {
    const timePeriodChips =
        timePicker.shadowRoot!.querySelectorAll<HTMLElement>(
            'cr-chip.time-period-chip');
    assertTrue(!!timePeriodChips);
    assertEquals(timePeriodChips.length, timePeriods.length);

    // Verify a chip exists for every time period and non in the dropdown menu.
    for (const timePeriod of timePeriods) {
      assertTrue(!!getChipForTimePeriod(timePeriod));
      assertFalse(!!getMenuItemForTimePeriod(timePeriod));
    }
  }

  function verifyMenuItemsExistForTimePeriods(timePeriods: TimePeriod[]) {
    // Open the 'More' dropdown menu.
    timePicker.$.moreButton.click();
    flush();

    const menuItems =
        timePicker.shadowRoot!.querySelectorAll<HTMLElement>('.dropdown-item');
    assertTrue(!!menuItems);
    assertEquals(menuItems.length, timePeriods.length);

    // Verify a dropdown menu item exists for every time period and no chips.
    for (const timePeriod of timePeriods) {
      assertTrue(!!getMenuItemForTimePeriod(timePeriod));
      assertFalse(!!getChipForTimePeriod(timePeriod));
    }
  }

  test('SelectTimePeriodFromChips', async function() {
    // Select Time Period from available chips.
    const targetChip = getChipForTimePeriod(TimePeriod.LAST_15_MINUTES);
    assertTrue(!!targetChip);
    targetChip.click();
    await flushTasks();

    // LAST_15_MINUTES chip should be selected.
    const selectedChip = getSelectedChip();
    assertEquals(selectedChip, targetChip);

    verifyChipsExistForTimePeriods([
      TimePeriod.LAST_15_MINUTES,
      TimePeriod.LAST_HOUR,
      TimePeriod.LAST_DAY,
      TimePeriod.LAST_WEEK,
    ]);
    verifyMenuItemsExistForTimePeriods(
        [TimePeriod.FOUR_WEEKS, TimePeriod.ALL_TIME]);

    // Verify the pref value was not modified during selection.
    assertEquals(
        TimePeriod.LAST_HOUR,
        timePicker.getPref('browser.clear_data.time_period').value);
  });

  test('SelectTimePeriodFromMenu', async function() {
    // Select Time Period from dropdown menu.
    const targetMenuItem = getMenuItemForTimePeriod(TimePeriod.ALL_TIME);
    assertTrue(!!targetMenuItem);
    targetMenuItem.click();
    await flushTasks();

    // ALL_TIME chip should be selected.
    const selectedTimePeriodChip = getSelectedChip();
    assertTrue(!!selectedTimePeriodChip);
    assertEquals(
        selectedTimePeriodChip.textContent!.trim(),
        getTimePeriodString(TimePeriod.ALL_TIME));

    verifyChipsExistForTimePeriods([
      TimePeriod.LAST_15_MINUTES,
      TimePeriod.LAST_HOUR,
      TimePeriod.LAST_DAY,
      TimePeriod.ALL_TIME,
    ]);
    verifyMenuItemsExistForTimePeriods(
        [TimePeriod.LAST_WEEK, TimePeriod.FOUR_WEEKS]);

    // Verify the pref value was not modified during selection.
    assertEquals(
        TimePeriod.LAST_HOUR,
        timePicker.getPref('browser.clear_data.time_period').value);
  });

  test('PrefChangesUpdatesSelectedChip', async function() {
    timePicker.setPrefValue(
        'browser.clear_data.time_period', TimePeriod.FOUR_WEEKS);
    await flushTasks();

    const selectedChip = getSelectedChip();
    assertTrue(!!selectedChip);
    assertEquals(
        selectedChip.textContent!.trim(),
        getTimePeriodString(TimePeriod.FOUR_WEEKS));

    verifyChipsExistForTimePeriods([
      TimePeriod.LAST_15_MINUTES,
      TimePeriod.LAST_HOUR,
      TimePeriod.LAST_DAY,
      TimePeriod.FOUR_WEEKS,
    ]);
    verifyMenuItemsExistForTimePeriods(
        [TimePeriod.LAST_WEEK, TimePeriod.ALL_TIME]);
  });
});

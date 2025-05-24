// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import type {ClearBrowsingDataResult, SettingsCheckboxElement, SettingsClearBrowsingDataDialogV2Element} from 'chrome://settings/lazy_load.js';
import {BrowsingDataType, ClearBrowsingDataBrowserProxyImpl, getDataTypePrefName, getTimePeriodString, TimePeriod} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertArrayEquals, assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestClearBrowsingDataBrowserProxy} from './test_clear_browsing_data_browser_proxy.js';

// clang-format on

suite('DeleteBrowsingDataDialog', function() {
  let testClearBrowsingDataBrowserProxy: TestClearBrowsingDataBrowserProxy;
  let dialog: SettingsClearBrowsingDataDialogV2Element;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    testClearBrowsingDataBrowserProxy = new TestClearBrowsingDataBrowserProxy();
    ClearBrowsingDataBrowserProxyImpl.setInstance(
        testClearBrowsingDataBrowserProxy);
    setClearBrowsingDataPrefs(false);
    return createDialog();
  });

  function setClearBrowsingDataPrefs(enableCheckboxes: boolean) {
    settingsPrefs.set(
        `prefs.${getDataTypePrefName(BrowsingDataType.HISTORY)}.value`,
        enableCheckboxes);
    settingsPrefs.set(
        `prefs.${getDataTypePrefName(BrowsingDataType.SITE_DATA)}.value`,
        enableCheckboxes);
    settingsPrefs.set(
        `prefs.${getDataTypePrefName(BrowsingDataType.CACHE)}.value`,
        enableCheckboxes);
    settingsPrefs.set(
        `prefs.${getDataTypePrefName(BrowsingDataType.DOWNLOADS)}.value`,
        enableCheckboxes);
    settingsPrefs.set(
        `prefs.${getDataTypePrefName(BrowsingDataType.FORM_DATA)}.value`,
        enableCheckboxes);
    settingsPrefs.set(
        `prefs.${getDataTypePrefName(BrowsingDataType.SITE_SETTINGS)}.value`,
        enableCheckboxes);
    settingsPrefs.set(
        `prefs.${getDataTypePrefName(BrowsingDataType.HOSTED_APPS_DATA)}.value`,
        enableCheckboxes);
  }

  async function createDialog() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('settings-clear-browsing-data-dialog-v2');
    dialog.set('prefs', settingsPrefs.prefs);
    document.body.appendChild(dialog);
    return waitAfterNextRender(dialog);
  }

  function verifyCheckboxesVisibleForDataTypesInOrder(
      datatypes: BrowsingDataType[]) {
    const visibleCheckboxes =
        dialog.shadowRoot!.querySelectorAll<SettingsCheckboxElement>(
            'settings-checkbox');
    assertTrue(!!visibleCheckboxes);
    assertEquals(datatypes.length, visibleCheckboxes.length);

    for (let i = 0; i < visibleCheckboxes.length; ++i) {
      assertEquals(
          getDataTypePrefName(datatypes[i]!), visibleCheckboxes[i]!.pref!.key);
    }
  }

  function getCheckboxForDataType(datatype: BrowsingDataType):
      SettingsCheckboxElement|undefined {
    const visibleCheckboxes =
        dialog.shadowRoot!.querySelectorAll<SettingsCheckboxElement>(
            'settings-checkbox');
    assertTrue(!!visibleCheckboxes);

    for (const checkbox of visibleCheckboxes) {
      if (checkbox.pref!.key === getDataTypePrefName(datatype)) {
        return checkbox;
      }
    }
    return undefined;
  }

  async function selectTimePeriodFromTimePicker(timePeriod: TimePeriod) {
    const visibleTimePeriodChips =
        dialog.$.timePicker.shadowRoot!.querySelectorAll<HTMLElement>(
            'cr-chip');
    assertTrue(!!visibleTimePeriodChips);

    for (const chip of visibleTimePeriodChips) {
      if (chip.textContent!.trim() === getTimePeriodString(timePeriod)) {
        chip.click();
        return;
      }
    }

    // Open the 'More' dropdown menu.
    dialog.$.timePicker.$.moreButton.click();
    await flushTasks();
    const menuItems =
        dialog.$.timePicker.shadowRoot!.querySelectorAll<HTMLElement>(
            '.dropdown-item');
    assertTrue(!!menuItems);

    for (const item of menuItems) {
      if (item.textContent!.trim() === getTimePeriodString(timePeriod)) {
        item.click();
        return;
      }
    }

    assertNotReached();
  }

  test('CancelButton', async function() {
    dialog.$.cancelButton.click();
    await eventToPromise('close', dialog);
  });

  test('DeleteButton', async function() {
    // Initially the button is disabled because no checkbox is selected and the
    // spinner is not visible.
    assertTrue(dialog.$.deleteButton.disabled);
    assertFalse(dialog.$.cancelButton.disabled);
    assertFalse(isVisible(dialog.$.spinner));

    // The button should be enabled if a checkbox is selected.
    const historyCheckbox = getCheckboxForDataType(BrowsingDataType.HISTORY);
    assertTrue(!!historyCheckbox);
    historyCheckbox.$.checkbox.click();
    await flushTasks();
    assertFalse(dialog.$.deleteButton.disabled);

    const promiseResolver = new PromiseResolver<ClearBrowsingDataResult>();
    testClearBrowsingDataBrowserProxy.setClearBrowsingDataPromise(
        promiseResolver.promise);

    // While the deletion is in progress, the Cancel and Delete button should be
    // disabled and the spinner should be visible.
    dialog.$.deleteButton.click();
    await testClearBrowsingDataBrowserProxy.whenCalled('clearBrowsingData');
    await flushTasks();
    assertTrue(dialog.$.deleteButton.disabled);
    assertTrue(dialog.$.cancelButton.disabled);
    assertTrue(isVisible(dialog.$.spinner));

    promiseResolver.resolve(
        {showHistoryNotice: false, showPasswordsNotice: false});
  });

  test('ShowMoreButton', function() {
    assertTrue(isVisible(dialog.$.showMoreButton));

    dialog.$.showMoreButton.click();
    assertFalse(isVisible(dialog.$.showMoreButton));
  });

  test('ExpandableCheckboxes', async function() {
    // Case 1, no checkbox is selected, only default checkboxes should be
    // expanded by default.
    verifyCheckboxesVisibleForDataTypesInOrder([
      BrowsingDataType.HISTORY,
      BrowsingDataType.SITE_DATA,
      BrowsingDataType.CACHE,
    ]);

    dialog.$.showMoreButton.click();
    await flushTasks();
    // On show more click, all checkboxes should be visible in default order.
    verifyCheckboxesVisibleForDataTypesInOrder([
      BrowsingDataType.HISTORY,
      BrowsingDataType.SITE_DATA,
      BrowsingDataType.CACHE,
      BrowsingDataType.DOWNLOADS,
      BrowsingDataType.FORM_DATA,
      BrowsingDataType.SITE_SETTINGS,
      BrowsingDataType.HOSTED_APPS_DATA,
    ]);

    // Case 2, some checkboxes are selected, default and selected checkboxes
    // should be expanded by default.
    settingsPrefs.set(
        `prefs.${getDataTypePrefName(BrowsingDataType.CACHE)}.value`, true);
    settingsPrefs.set(
        `prefs.${getDataTypePrefName(BrowsingDataType.DOWNLOADS)}.value`, true);
    settingsPrefs.set(
        `prefs.${getDataTypePrefName(BrowsingDataType.HOSTED_APPS_DATA)}.value`,
        true);
    await createDialog();

    verifyCheckboxesVisibleForDataTypesInOrder([
      BrowsingDataType.HISTORY,
      BrowsingDataType.SITE_DATA,
      BrowsingDataType.CACHE,
      BrowsingDataType.DOWNLOADS,
      BrowsingDataType.HOSTED_APPS_DATA,
    ]);

    dialog.$.showMoreButton.click();
    await flushTasks();
    // On show more click, all checkboxes should be visible with the unselected
    // checkboxes at the bottom.
    verifyCheckboxesVisibleForDataTypesInOrder([
      BrowsingDataType.HISTORY,
      BrowsingDataType.SITE_DATA,
      BrowsingDataType.CACHE,
      BrowsingDataType.DOWNLOADS,
      BrowsingDataType.HOSTED_APPS_DATA,
      BrowsingDataType.FORM_DATA,
      BrowsingDataType.SITE_SETTINGS,
    ]);

    // Case 3, All checkboxes are selected, all checkboxes should be expanded by
    // default and "Show more" button should be hidden.
    setClearBrowsingDataPrefs(true);
    await createDialog();

    verifyCheckboxesVisibleForDataTypesInOrder([
      BrowsingDataType.HISTORY,
      BrowsingDataType.SITE_DATA,
      BrowsingDataType.CACHE,
      BrowsingDataType.DOWNLOADS,
      BrowsingDataType.FORM_DATA,
      BrowsingDataType.SITE_SETTINGS,
      BrowsingDataType.HOSTED_APPS_DATA,
    ]);
    assertFalse(isVisible(dialog.$.showMoreButton));
  });

  test('CheckboxSelection', async function() {
    // Case 1, selection from expanded checkboxes.
    settingsPrefs.set(
        `prefs.${getDataTypePrefName(BrowsingDataType.DOWNLOADS)}.value`, true);
    await createDialog();

    // Only Downloads Checkbox is selected, only default and the Downloads
    // checkboxes should be visible.
    verifyCheckboxesVisibleForDataTypesInOrder([
      BrowsingDataType.HISTORY,
      BrowsingDataType.SITE_DATA,
      BrowsingDataType.CACHE,
      BrowsingDataType.DOWNLOADS,
    ]);

    const expandedCheckbox = getCheckboxForDataType(BrowsingDataType.DOWNLOADS);
    assertTrue(!!expandedCheckbox);
    assertTrue(expandedCheckbox.pref!.value);

    expandedCheckbox.$.checkbox.click();
    await expandedCheckbox.$.checkbox.updateComplete;

    // Checkbox should now be unselected.
    assertFalse(expandedCheckbox.checked);
    // Associated pref should not change on checkbox selection.
    assertTrue(expandedCheckbox.pref!.value);

    // Checkboxes order should remain unchanged.
    verifyCheckboxesVisibleForDataTypesInOrder([
      BrowsingDataType.HISTORY,
      BrowsingDataType.SITE_DATA,
      BrowsingDataType.CACHE,
      BrowsingDataType.DOWNLOADS,
    ]);

    // Case 2, selection from more checkboxes.
    dialog.$.showMoreButton.click();
    await flushTasks();

    // All checkboxes should be visible.
    verifyCheckboxesVisibleForDataTypesInOrder([
      BrowsingDataType.HISTORY,
      BrowsingDataType.SITE_DATA,
      BrowsingDataType.CACHE,
      BrowsingDataType.DOWNLOADS,
      BrowsingDataType.FORM_DATA,
      BrowsingDataType.SITE_SETTINGS,
      BrowsingDataType.HOSTED_APPS_DATA,
    ]);

    const moreCheckbox =
        getCheckboxForDataType(BrowsingDataType.HOSTED_APPS_DATA);
    assertTrue(!!moreCheckbox);
    assertFalse(moreCheckbox.pref!.value);

    moreCheckbox.$.checkbox.click();
    await moreCheckbox.$.checkbox.updateComplete;

    // Checkbox should now be selected.
    assertTrue(moreCheckbox.checked);
    // Associated pref should not change on checkbox selection.
    assertFalse(moreCheckbox.pref!.value);

    // Checkboxes order should remain unchanged.
    verifyCheckboxesVisibleForDataTypesInOrder([
      BrowsingDataType.HISTORY,
      BrowsingDataType.SITE_DATA,
      BrowsingDataType.CACHE,
      BrowsingDataType.DOWNLOADS,
      BrowsingDataType.FORM_DATA,
      BrowsingDataType.SITE_SETTINGS,
      BrowsingDataType.HOSTED_APPS_DATA,
    ]);
  });

  test('PrefChangeDoesNotUpdateCheckboxOrder', async function() {
    // No checkbox is selected, only default checkboxes are visible.
    verifyCheckboxesVisibleForDataTypesInOrder([
      BrowsingDataType.HISTORY,
      BrowsingDataType.SITE_DATA,
      BrowsingDataType.CACHE,
    ]);

    settingsPrefs.set(
        `prefs.${getDataTypePrefName(BrowsingDataType.FORM_DATA)}.value`, true);
    settingsPrefs.set(
        `prefs.${getDataTypePrefName(BrowsingDataType.HOSTED_APPS_DATA)}.value`,
        true);
    await flushTasks();

    // Pref changes should not change checkbox expansion state.
    verifyCheckboxesVisibleForDataTypesInOrder([
      BrowsingDataType.HISTORY,
      BrowsingDataType.SITE_DATA,
      BrowsingDataType.CACHE,
    ]);

    dialog.$.showMoreButton.click();
    await flushTasks();

    const formDataCheckbox = getCheckboxForDataType(BrowsingDataType.FORM_DATA);
    assertTrue(!!formDataCheckbox);
    assertTrue(formDataCheckbox.checked);

    const hostedAppsDataCheckbox =
        getCheckboxForDataType(BrowsingDataType.HOSTED_APPS_DATA);
    assertTrue(!!hostedAppsDataCheckbox);
    assertTrue(hostedAppsDataCheckbox.checked);

    // Checkbox order should not change on pref changes.
    verifyCheckboxesVisibleForDataTypesInOrder([
      BrowsingDataType.HISTORY,
      BrowsingDataType.SITE_DATA,
      BrowsingDataType.CACHE,
      BrowsingDataType.DOWNLOADS,
      BrowsingDataType.FORM_DATA,
      BrowsingDataType.SITE_SETTINGS,
      BrowsingDataType.HOSTED_APPS_DATA,
    ]);
  });

  test('BrowsingDataTypePrefs', function() {
    assertEquals(
        'browser.clear_data.browsing_history',
        getDataTypePrefName(BrowsingDataType.HISTORY));
    assertEquals(
        'browser.clear_data.cookies',
        getDataTypePrefName(BrowsingDataType.SITE_DATA));
    assertEquals(
        'browser.clear_data.cache',
        getDataTypePrefName(BrowsingDataType.CACHE));
    assertEquals(
        'browser.clear_data.form_data',
        getDataTypePrefName(BrowsingDataType.FORM_DATA));
    assertEquals(
        'browser.clear_data.site_settings',
        getDataTypePrefName(BrowsingDataType.SITE_SETTINGS));
    assertEquals(
        'browser.clear_data.download_history',
        getDataTypePrefName(BrowsingDataType.DOWNLOADS));
    assertEquals(
        'browser.clear_data.hosted_apps_data',
        getDataTypePrefName(BrowsingDataType.HOSTED_APPS_DATA));
  });

  test('TimePeriodChangesRestartCounters', async function() {
    // Clear previous restartCounters calls.
    testClearBrowsingDataBrowserProxy.reset();

    // Set the selected TimePeriod to LAST_WEEK.
    await selectTimePeriodFromTimePicker(TimePeriod.LAST_WEEK);
    await flushTasks();

    const args =
        await testClearBrowsingDataBrowserProxy.whenCalled('restartCounters');
    assertEquals(args[0], /*isBasic=*/ false);
    assertEquals(args[1], TimePeriod.LAST_WEEK);
  });

  test('CountersUpdateCheckboxSubLabel', async function() {
    // Case 1, Counter updates a checkbox in the expanded options list.
    // Simulate a browsing data counter result for history. The History
    // checkbox's subLabel should be updated.
    webUIListenerCallback(
        'browsing-data-counter-text-update',
        'browser.clear_data.browsing_history', 'history result');
    await flushTasks();

    const historyCheckbox = getCheckboxForDataType(BrowsingDataType.HISTORY);
    assertTrue(!!historyCheckbox);
    assertEquals('history result', historyCheckbox.subLabel);

    // Case 2, Counter updates a checkbox in the more options list.
    // Simulate a browsing data counter result for Site settings. The Site
    // settings checkbox's subLabel should be updated.
    webUIListenerCallback(
        'browsing-data-counter-text-update', 'browser.clear_data.site_settings',
        'site settings result');

    dialog.$.showMoreButton.click();
    await flushTasks();

    const siteSettingsCheckbox =
        getCheckboxForDataType(BrowsingDataType.SITE_SETTINGS);
    assertTrue(!!siteSettingsCheckbox);
    assertEquals('site settings result', siteSettingsCheckbox.subLabel);
  });

  test('ClearBrowsingData', async function() {
    // Update the selected TimePeriod to LAST_DAY.
    await selectTimePeriodFromTimePicker(TimePeriod.LAST_DAY);

    // Select datatypes for deletion.
    dialog.$.showMoreButton.click();
    await flushTasks();
    const historyCheckbox = getCheckboxForDataType(BrowsingDataType.HISTORY);
    assertTrue(!!historyCheckbox);
    historyCheckbox.$.checkbox.click();

    const downloadsCheckbox =
        getCheckboxForDataType(BrowsingDataType.DOWNLOADS);
    assertTrue(!!downloadsCheckbox);
    downloadsCheckbox.$.checkbox.click();

    const hostedAppsDataCheckbox =
        getCheckboxForDataType(BrowsingDataType.HOSTED_APPS_DATA);
    assertTrue(!!hostedAppsDataCheckbox);
    hostedAppsDataCheckbox.$.checkbox.click();
    await flushTasks();

    // Trigger the deletion.
    const promiseResolver = new PromiseResolver<ClearBrowsingDataResult>();
    testClearBrowsingDataBrowserProxy.setClearBrowsingDataPromise(
        promiseResolver.promise);
    dialog.$.deleteButton.click();

    // Verify TimePeriod pref is updated.
    assertEquals(
        TimePeriod.LAST_DAY,
        dialog.getPref('browser.clear_data.time_period').value);

    // Verify DataType prefs are updated.
    assertEquals(
        true, dialog.getPref('browser.clear_data.browsing_history').value);
    assertEquals(false, dialog.getPref('browser.clear_data.cookies').value);
    assertEquals(false, dialog.getPref('browser.clear_data.cache').value);
    assertEquals(false, dialog.getPref('browser.clear_data.form_data').value);
    assertEquals(
        false, dialog.getPref('browser.clear_data.site_settings').value);
    assertEquals(
        true, dialog.getPref('browser.clear_data.download_history').value);
    assertEquals(
        true, dialog.getPref('browser.clear_data.hosted_apps_data').value);

    // Verify correct TimePeriod and DataTypes are sent to the proxy.
    const args =
        await testClearBrowsingDataBrowserProxy.whenCalled('clearBrowsingData');
    const dataTypes = args[0];
    assertEquals(3, dataTypes.length);
    const expectedDataTypes = [
      'browser.clear_data.browsing_history',
      'browser.clear_data.download_history',
      'browser.clear_data.hosted_apps_data',
    ];
    assertArrayEquals(expectedDataTypes, dataTypes);

    const timePeriod = args[1];
    assertEquals(TimePeriod.LAST_DAY, timePeriod);

    // Simulate that the deletion has completed.
    promiseResolver.resolve(
        {showHistoryNotice: false, showPasswordsNotice: false});
    await promiseResolver.promise;

    // Verify dialog is closed after deletion is completed.
    assertFalse(dialog.$.deleteBrowsingDataDialog.open);
  });
});

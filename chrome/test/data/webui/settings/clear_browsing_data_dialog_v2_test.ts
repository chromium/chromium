// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {SettingsCheckboxElement, SettingsClearBrowsingDataDialogV2Element} from 'chrome://settings/lazy_load.js';
import {BrowsingDataType, getDataTypePrefName} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

// clang-format on

suite('DeleteBrowsingDataDialog', function() {
  let dialog: SettingsClearBrowsingDataDialogV2Element;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
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
    return flushTasks();
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

  test('CancelButton', async function() {
    dialog.$.cancelButton.click();
    await eventToPromise('close', dialog);
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
});

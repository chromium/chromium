// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsOfficePageElement} from 'chrome://os-settings/lazy_load.js';
import {CrSettingsPrefs, SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';


suite('<settings-office-page>', function() {
  let page: SettingsOfficePageElement;
  let prefElement: SettingsPrefsElement;

  setup(async function() {
    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    page = document.createElement('settings-office-page');
    page.prefs = prefElement.prefs;
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
    prefElement.remove();
  });

  test('Unchecked when always move to Drive pref is true', function() {
    page.setPrefValue('filebrowser.office.always_move_to_drive', true);
    flush();

    const askToMove =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#alwaysMoveToDrive');
    assert(askToMove);
    assertFalse(askToMove.checked);
  });

  test('Unchecked when always move to OneDrive pref is true', function() {
    page.setPrefValue('filebrowser.office.always_move_to_onedrive', true);
    flush();

    const askToMove =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#alwaysMoveToOneDrive');
    assert(askToMove);
    assertFalse(askToMove.checked);
  });

  test('Checked when always move to Drive pref is false', function() {
    page.setPrefValue('filebrowser.office.always_move_to_drive', false);
    flush();

    const askToMove =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#alwaysMoveToDrive');
    assert(askToMove);
    assertTrue(askToMove.checked);
  });

  test('Checked when always move to OneDrive pref is false', function() {
    page.setPrefValue('filebrowser.office.always_move_to_onedrive', false);
    flush();

    const askToMove =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#alwaysMoveToOneDrive');
    assert(askToMove);
    assertTrue(askToMove.checked);
  });

  test('Sets Drive pref to false when clicked from true', function() {
    page.setPrefValue('filebrowser.office.always_move_to_drive', true);
    flush();

    const askToMove =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#alwaysMoveToDrive');
    assert(askToMove);
    assertFalse(askToMove.checked);

    askToMove.click();
    assertTrue(askToMove.checked);
    assertFalse(page.getPref('filebrowser.office.always_move_to_drive').value);
  });

  test('Sets OneDrive pref to false when clicked from true', function() {
    page.setPrefValue('filebrowser.office.always_move_to_onedrive', true);
    flush();

    const askToMove =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#alwaysMoveToOneDrive');
    assert(askToMove);
    assertFalse(askToMove.checked);

    askToMove.click();
    assertTrue(askToMove.checked);
    assertFalse(
        page.getPref('filebrowser.office.always_move_to_onedrive').value);
  });

  test('Sets Drive pref to true when clicked from false', function() {
    page.setPrefValue('filebrowser.office.always_move_to_drive', false);
    flush();

    const askToMove =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#alwaysMoveToDrive');
    assert(askToMove);
    assertTrue(askToMove.checked);

    askToMove.click();
    assertFalse(askToMove.checked);
    assertTrue(page.getPref('filebrowser.office.always_move_to_drive').value);
  });

  test('Sets OneDrive pref to true when clicked from false', function() {
    page.setPrefValue('filebrowser.office.always_move_to_onedrive', false);
    flush();

    const askToMove =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#alwaysMoveToOneDrive');
    assert(askToMove);
    assertTrue(askToMove.checked);

    askToMove.click();
    assertFalse(askToMove.checked);
    assertTrue(
        page.getPref('filebrowser.office.always_move_to_onedrive').value);
  });
});

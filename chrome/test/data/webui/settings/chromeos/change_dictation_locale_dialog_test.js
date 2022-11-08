// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/os_settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('DictationChangeLanguageLocaleDialogTest', function() {
  /** @type {!ChangeDictationLocaleDialog} */
  let dialog;

  /** @type {!Array<!DictationLocaleOption>} */
  const options = [
    {name: 'English (US)', value: 'en-US', offline: true, recommended: true},
    {name: 'English (UK)', value: 'en-GB', offline: false, recommended: true},
    {name: 'Spanish', value: 'es-ES', offline: false, recommended: false},
  ];

  /** @type {!chrome.settingsPrivate.PrefObject} */
  const pref = {
    value: 'en-US',
  };

  setup(function() {
    dialog =
        document.createElement('os-settings-change-dictation-locale-dialog');
    dialog.pref = pref;
    dialog.options = options;
    document.body.appendChild(dialog);
    flush();
  });

  test('Cancel button closes dialog', function() {
    assertTrue(dialog.$.changeDictationLocaleDialog.open);

    const cancelBtn = dialog.$.cancel;
    assertTrue(!!cancelBtn);

    cancelBtn.click();
    assertFalse(dialog.$.changeDictationLocaleDialog.open);

    // Pref is unchanged.
    assertEquals('en-US', pref.value);
  });

  test('Shows recommended and all options', async () => {
    const recommendedList = dialog.$.recommendedLocalesList;
    assertTrue(!!recommendedList);
    // Two possible recommended items.
    assertEquals(2, recommendedList.items.length);
    // Nothing has been selected yet.
    assertFalse(!!recommendedList.selectedItem);

    const allList = dialog.$.allLocalesList;
    assertTrue(!!allList);
    // All three items are shown.
    assertEquals(3, allList.items.length);
    // Nothing has been selected yet.
    assertFalse(!!allList.selectedItem);
  });

  test('Selects recommended option and saves', async () => {
    const recommendedList = dialog.$.recommendedLocalesList;
    const allList = dialog.$.allLocalesList;
    assertTrue(!!recommendedList);
    assertTrue(!!allList);

    recommendedList.selectIndex(0);

    // en-GB was selected.
    assertTrue(!!recommendedList.selectedItem);
    assertTrue(!!allList.selectedItem);
    assertEquals('en-GB', recommendedList.selectedItem.value);
    assertEquals('en-GB', allList.selectedItem.value);

    // Clicking the update button updates the pref.
    const updateBtn = dialog.$.update;
    assertTrue(!!updateBtn);
    updateBtn.click();
    assertFalse(dialog.$.changeDictationLocaleDialog.open);
    assertEquals('en-GB', pref.value);
  });

  test('Selects from all options', async () => {
    const recommendedList = dialog.$.recommendedLocalesList;
    const allList = dialog.$.allLocalesList;
    assertTrue(!!recommendedList);
    assertTrue(!!allList);

    allList.selectIndex(0);

    // en-GB was selected.
    assertTrue(!!recommendedList.selectedItem);
    assertTrue(!!allList.selectedItem);
    assertEquals('en-GB', recommendedList.selectedItem.value);
    assertEquals('en-GB', allList.selectedItem.value);

    allList.selectIndex(2);
    assertTrue(!!allList.selectedItem);
    assertEquals('es-ES', allList.selectedItem.value);

    // No recommended item selected.
    assertFalse(!!recommendedList.selectedItem);

    // Clicking the update button updates the pref.
    const updateBtn = dialog.$.update;
    assertTrue(!!updateBtn);
    updateBtn.click();
    assertFalse(dialog.$.changeDictationLocaleDialog.open);
    assertEquals('es-ES', pref.value);
  });
});

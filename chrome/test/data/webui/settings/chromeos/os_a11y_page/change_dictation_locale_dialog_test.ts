// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {ChangeDictationLocaleDialog, DictationLocaleOption} from 'chrome://os-settings/lazy_load.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('<os-settings-change-dictation-locale-dialog>', () => {
  let dialog: ChangeDictationLocaleDialog;

  const options: DictationLocaleOption[] = [
    {
      name: 'English (US)',
      value: 'en-US',
      worksOffline: true,
      installed: true,
      recommended: true,
    },
    {
      name: 'English (UK)',
      value: 'en-GB',
      worksOffline: false,
      installed: true,
      recommended: true,
    },
    {
      name: 'Spanish',
      value: 'es-ES',
      worksOffline: false,
      installed: true,
      recommended: false,
    },
  ];

  const pref: chrome.settingsPrivate.PrefObject<string> = {
    key: 'test',
    type: chrome.settingsPrivate.PrefType.STRING,
    value: 'en-US',
  };

  setup(() => {
    dialog =
        document.createElement('os-settings-change-dictation-locale-dialog');
    dialog.pref = pref;
    dialog.options = options;
    document.body.appendChild(dialog);
    flush();
  });

  test('Cancel button closes dialog', () => {
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
    assertEquals(2, recommendedList.items!.length);
    // Nothing has been selected yet.
    assertEquals(null, recommendedList.selectedItem);

    const allList = dialog.$.allLocalesList;
    assertTrue(!!allList);
    // All three items are shown.
    assertEquals(3, allList.items!.length);
    // Nothing has been selected yet.
    assertEquals(null, allList.selectedItem);
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
    assertEquals(
        'en-GB', (recommendedList.selectedItem as DictationLocaleOption).value);
    assertEquals(
        'en-GB', (allList.selectedItem as DictationLocaleOption).value);

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
    assertEquals(
        'en-GB', (recommendedList.selectedItem as DictationLocaleOption).value);
    assertEquals(
        'en-GB', (allList.selectedItem as DictationLocaleOption).value);

    allList.selectIndex(2);
    assertTrue(!!allList.selectedItem);
    assertEquals(
        'es-ES', (allList.selectedItem as DictationLocaleOption).value);

    // No recommended item selected.
    assertEquals(null, recommendedList.selectedItem);

    // Clicking the update button updates the pref.
    const updateBtn = dialog.$.update;
    assertTrue(!!updateBtn);
    updateBtn.click();
    assertFalse(dialog.$.changeDictationLocaleDialog.open);
    assertEquals('es-ES', pref.value);
  });
});

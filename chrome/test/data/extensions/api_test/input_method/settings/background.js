// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  // Test getting the settings for an engine with no settings.
  function getUnsetKey() {
    chrome.inputMethodPrivate.getSettings('test', (val) => {
      chrome.test.assertEq(null, val);
      chrome.test.succeed();
    });
  },
  // Test setting and getting the settings.
  function getSetSettings() {
    const settings = {
      'koreanKeyboardLayout': 'set 2',
      'zhuyinPageSize': 7,
      'enableDoubleSpacePeriod': true
    }
    chrome.inputMethodPrivate.setSettings('test', settings, () => {
      chrome.inputMethodPrivate.getSettings('test', (val) => {
        chrome.test.assertEq(settings, val);
        chrome.test.succeed();
      });
    });
  },
  // Test updating settings.
  function updateKey() {
    const settingsBefore = { 'zhuyinPageSize': 7 };
    const settingsAfter = { 'zhuyinPageSize': 9 };
    chrome.inputMethodPrivate.setSettings('test', settingsBefore, () => {
      chrome.inputMethodPrivate.setSettings('test', settingsAfter, () => {
        chrome.inputMethodPrivate.getSettings('test', (val) => {
          chrome.test.assertEq(settingsAfter, val);
          chrome.test.succeed();
        });
      });
    });
  },
  // Test setting and getting for different IMEs.
  function getSetSameKeyDifferentIMEs() {
    const settings1 = { 'enableDoubleSpacePeriod': true };
    const settings2 = { 'enableDoubleSpacePeriod': false };
    chrome.inputMethodPrivate.setSettings('ime1', settings1, () => {
      chrome.inputMethodPrivate.setSettings('ime2', settings2, () => {
        chrome.inputMethodPrivate.getSettings('ime1', (val) => {
          chrome.test.assertEq(settings1, val);
          chrome.inputMethodPrivate.getSettings('ime2', (val) => {
            chrome.test.assertEq(settings2, val);
            chrome.test.succeed();
          });
        });
      });
    });
  },
  // Test OnSettingsChanged event gets raised when settings are updated.
  function eventRaisedWhenSettingToInitialValue() {
    const settings = { 'enableDoubleSpacePeriod': true };
    const listener = (ime) => {
      chrome.test.assertEq('ime', ime);
      chrome.test.succeed();

      chrome.inputMethodPrivate.onInputMethodOptionsChanged
          .removeListener(listener);
    };

    chrome.inputMethodPrivate.onInputMethodOptionsChanged.addListener(listener);
    chrome.inputMethodPrivate.setSettings('ime', settings);
  }
]);

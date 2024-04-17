// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Font settings API test
// browser_tests --gtest_filter=ExtensionApiTest.FontSettingsGenericFamilies

var fs = chrome.fontSettings;
var CONTROLLED_BY_THIS_EXTENSION = 'controlled_by_this_extension';
var CONTROLLABLE_BY_THIS_EXTENSION = 'controllable_by_this_extension';

// TODO(crbug.com/40187445): Support generic font families added to CSS Fonts
// Module Level 4.
const genericFamilyNames = [
  'standard',
  'serif',
  'sansserif',
  'cursive',
  'fantasy',
  'fixed',
  'math',
];

chrome.test.runTests([
  async function setGenericFamilies() {
    for (const genericFamily of genericFamilyNames) {
      const fontId = `custom_${genericFamily}`;
      let value = null;
      await new Promise(async resolve => {
        let listener = details => {
          value = details;
          fs.onFontChanged.removeListener(listener);
          resolve();
        };
        fs.onFontChanged.addListener(listener);
        try {
          await fs.setFont({genericFamily: genericFamily, fontId: fontId});
        } catch (error) {
          chrome.test.fail(error);
          resolve();
        }
      });
      chrome.test.assertEq(
          {
            genericFamily: genericFamily,
            fontId: fontId,
            script: 'Zyyy',
            levelOfControl: CONTROLLED_BY_THIS_EXTENSION
          },
          value);
    }
    chrome.test.succeed();
  },

  async function getGenericFamilies() {
    for (const genericFamily of genericFamilyNames) {
      const fontId = `custom_${genericFamily}`;
      try {
        await fs.getFont({genericFamily: genericFamily}).then(value => {
          chrome.test.assertEq(
              {fontId: fontId, levelOfControl: CONTROLLED_BY_THIS_EXTENSION},
              value);
        });
      } catch (error) {
        chrome.test.fail(error);
      }
    }
    chrome.test.succeed();
  },

  async function clearGenericFamilies() {
    for (const genericFamily of genericFamilyNames) {
      const fontId = `default_${genericFamily}`
      let value = null;
      await new Promise(async resolve => {
        let listener = details => {
          value = details;
          fs.onFontChanged.removeListener(listener);
          resolve();
        };
        fs.onFontChanged.addListener(listener);
        try {
          await fs.clearFont({genericFamily: genericFamily});
        } catch (error) {
          chrome.test.fail(error);
          resolve();
        }
      });
      chrome.test.assertEq(
          {
            script: 'Zyyy',
            genericFamily: genericFamily,
            fontId: fontId,
            levelOfControl: CONTROLLABLE_BY_THIS_EXTENSION
          },
          value);
    }
    chrome.test.succeed();
  },
]);

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Font settings API test for split mode (in incognito context)
// Run with browser_tests --gtest_filter=ExtensionApiTest.FontSettingsIncognito

const fs = chrome.fontSettings;

const CONTROLLABLE_BY_THIS_EXTENSION = 'controllable_by_this_extension';
const SET_FROM_INCOGNITO_ERROR =
    'Can\'t modify regular settings from an incognito context.';

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

chrome.test.runTests([
  function setPerScriptFont() {
    const script = 'Hang';
    const genericFamily = 'standard';
    const fontId = 'Verdana';

    fs.setFont(
        {
          script: script,
          genericFamily: genericFamily,
          fontId: fontId,
        },
        chrome.test.callbackFail(SET_FROM_INCOGNITO_ERROR));
  },

  function setGlobalFontName() {
    const genericFamily = 'sansserif';
    const fontId = 'Tahoma';

    fs.setFont(
        {
          genericFamily: genericFamily,
          fontId: fontId,
        },
        chrome.test.callbackFail(SET_FROM_INCOGNITO_ERROR));
  },

  function setDefaultFontSize() {
    const pixelSize = 22;

    fs.setDefaultFontSize(
        {
          pixelSize: pixelSize,
        },
        chrome.test.callbackFail(SET_FROM_INCOGNITO_ERROR));
  },

  function getFontList() {
    const message = 'getFontList should return an array of objects with ' +
        'fontId and displayName properties.';
    const getPlatformInfo = new Promise((resolve) => {
      chrome.runtime.getPlatformInfo(info => resolve(info.os == 'android'));
    });
    fs.getFontList(chrome.test.callbackPass(function(value) {
      getPlatformInfo.then(isAndroid => {
        if (isAndroid) {
          // Android does not support a mechanism to get "all installed fonts"
          // like Windows/Mac/Linux.
          chrome.test.assertTrue(
              value.length === 0, 'Font list should be empty');
        } else {
          chrome.test.assertTrue(
              value.length > 0, 'Font list is not expected to be empty.');
          chrome.test.assertEq('string', typeof (value[0].fontId), message);
          chrome.test.assertEq(
              'string', typeof (value[0].displayName), message);
        }
      });
    }));
  },

  function getPerScriptFontName() {
    fs.getFont(
        {
          script: 'Hang',
          genericFamily: 'standard',
        },
        expect({
          fontId: 'Tahoma',
          levelOfControl: CONTROLLABLE_BY_THIS_EXTENSION,
        }));
  },

  function getGlobalFontName() {
    fs.getFont(
        {
          genericFamily: 'sansserif',
        },
        expect({
          fontId: 'Arial',
          levelOfControl: CONTROLLABLE_BY_THIS_EXTENSION,
        }));
  },

  function getDefaultFontSize() {
    fs.getDefaultFontSize({}, expect({
                            pixelSize: 16,
                            levelOfControl: CONTROLLABLE_BY_THIS_EXTENSION,
                          }));
  },

  function clearPerScriptFont() {
    const script = 'Hang';
    const genericFamily = 'standard';

    fs.clearFont(
        {
          script: script,
          genericFamily: genericFamily,
        },
        chrome.test.callbackFail(SET_FROM_INCOGNITO_ERROR));
  },

  function clearGlobalFont() {
    const genericFamily = 'sansserif';

    fs.clearFont(
        {
          genericFamily: genericFamily,
        },
        chrome.test.callbackFail(SET_FROM_INCOGNITO_ERROR));
  },

  function clearDefaultFontSize() {
    fs.clearDefaultFontSize(
        {}, chrome.test.callbackFail(SET_FROM_INCOGNITO_ERROR));
  },
]);

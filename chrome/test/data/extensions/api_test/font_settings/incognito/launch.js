// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Font settings API test for split mode (in incognito context)
// Run with browser_tests --gtest_filter=ExtensionApiTest.FontSettingsIncognito

var fs = chrome.fontSettings;

var CONTROLLABLE_BY_THIS_EXTENSION = 'controllable_by_this_extension';
var SET_FROM_INCOGNITO_ERROR =
    "Can't modify regular settings from an incognito context.";

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

chrome.test.runTests([
  function setPerScriptFont() {
    var script = 'Hang';
    var genericFamily = 'standard';
    var fontId = 'Verdana';

    fs.setFont({
      script: script,
      genericFamily: genericFamily,
      fontId: fontId
    }, chrome.test.callbackFail(SET_FROM_INCOGNITO_ERROR));
  },

  function setGlobalFontName() {
    var genericFamily = 'sansserif';
    var fontId = 'Tahoma';

    fs.setFont({
      genericFamily: genericFamily,
      fontId: fontId
    }, chrome.test.callbackFail(SET_FROM_INCOGNITO_ERROR));
  },

  function setDefaultFontSize() {
    var pixelSize = 22;

    fs.setDefaultFontSize({
      pixelSize: pixelSize
    }, chrome.test.callbackFail(SET_FROM_INCOGNITO_ERROR));
  },

  function getFontList() {
    var message = 'getFontList should return an array of objects with ' +
        'fontId and displayName properties.';
    fs.getFontList(chrome.test.callbackPass(function(value) {
      chrome.test.assertTrue(value.length > 0,
                             "Font list is not expected to be empty.");
      chrome.test.assertEq('string', typeof(value[0].fontId), message);
      chrome.test.assertEq('string', typeof(value[0].displayName), message);
    }));
  },

  function getPerScriptFontName() {
    fs.getFont({
      script: 'Hang',
      genericFamily: 'standard'
    }, expect({
      fontId: 'Tahoma',
      levelOfControl: CONTROLLABLE_BY_THIS_EXTENSION
    }));
  },

  function getGlobalFontName() {
    fs.getFont({
      genericFamily: 'sansserif'
    }, expect({
      fontId: 'Arial',
      levelOfControl: CONTROLLABLE_BY_THIS_EXTENSION
    }));
  },

  function getDefaultFontSize() {
    fs.getDefaultFontSize({}, expect({
      pixelSize: 16,
      levelOfControl: CONTROLLABLE_BY_THIS_EXTENSION
    }));
  },

  function clearPerScriptFont() {
    var script = 'Hang';
    var genericFamily = 'standard';

    fs.clearFont({
      script: script,
      genericFamily: genericFamily,
    }, chrome.test.callbackFail(SET_FROM_INCOGNITO_ERROR));
  },

  function clearGlobalFont() {
    var genericFamily = 'sansserif';

   fs.clearFont({
      genericFamily: genericFamily,
    }, chrome.test.callbackFail(SET_FROM_INCOGNITO_ERROR));
  },

  function clearDefaultFontSize() {
    fs.clearDefaultFontSize({},
                            chrome.test.callbackFail(SET_FROM_INCOGNITO_ERROR));
  }
]);


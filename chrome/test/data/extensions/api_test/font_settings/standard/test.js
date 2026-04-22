// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Font settings API test
// Run with browser_tests --gtest_filter=ExtensionApiTest.FontSettings

const fs = chrome.fontSettings;
const CONTROLLED_BY_THIS_EXTENSION = 'controlled_by_this_extension';
const CONTROLLABLE_BY_THIS_EXTENSION = 'controllable_by_this_extension';

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

    chrome.test.listenOnce(fs.onFontChanged, function(details) {
      chrome.test.assertEq(
          {
            script: script,
            genericFamily: genericFamily,
            fontId: fontId,
            levelOfControl: CONTROLLED_BY_THIS_EXTENSION,
          },
          details);
    });

    fs.setFont(
        {
          script: script,
          genericFamily: genericFamily,
          fontId: fontId,
        },
        chrome.test.callbackPass());
  },

  function setGlobalFontName() {
    const genericFamily = 'sansserif';
    const fontId = 'Tahoma';

    chrome.test.listenOnce(fs.onFontChanged, function(details) {
      chrome.test.assertEq(
          {
            genericFamily: genericFamily,
            fontId: fontId,
            script: 'Zyyy',
            levelOfControl: CONTROLLED_BY_THIS_EXTENSION,
          },
          details);
    });

    fs.setFont(
        {
          genericFamily: genericFamily,
          fontId: fontId,
        },
        chrome.test.callbackPass());
  },

  function setDefaultFontSize() {
    const pixelSize = 22;
    chrome.test.listenOnce(fs.onDefaultFontSizeChanged, function(details) {
      chrome.test.assertEq(
          {
            pixelSize: pixelSize,
            levelOfControl: CONTROLLED_BY_THIS_EXTENSION,
          },
          details);
    });

    fs.setDefaultFontSize(
        {
          pixelSize: pixelSize,
        },
        chrome.test.callbackPass());
  },

  function setDefaultFixedFontSize() {
    const pixelSize = 42;
    chrome.test.listenOnce(fs.onDefaultFixedFontSizeChanged, function(details) {
      chrome.test.assertEq(
          {
            pixelSize: pixelSize,
            levelOfControl: CONTROLLED_BY_THIS_EXTENSION,
          },
          details);
    });

    fs.setDefaultFixedFontSize(
        {
          pixelSize: pixelSize,
        },
        chrome.test.callbackPass());
  },

  function setMinimumFontSize() {
    const pixelSize = 7;
    chrome.test.listenOnce(fs.onMinimumFontSizeChanged, function(details) {
      chrome.test.assertEq(
          {
            pixelSize: pixelSize,
            levelOfControl: CONTROLLED_BY_THIS_EXTENSION,
          },
          details);
    });

    fs.setMinimumFontSize(
        {
          pixelSize: pixelSize,
        },
        chrome.test.callbackPass());
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
          fontId: 'Verdana',
          levelOfControl: CONTROLLED_BY_THIS_EXTENSION,
        }));
  },

  function getGlobalFontName() {
    fs.getFont(
        {
          genericFamily: 'sansserif',
        },
        expect({
          fontId: 'Tahoma',
          levelOfControl: CONTROLLED_BY_THIS_EXTENSION,
        }));
  },

  function getDefaultFontSize() {
    fs.getDefaultFontSize({}, expect({
                            pixelSize: 22,
                            levelOfControl: CONTROLLED_BY_THIS_EXTENSION,
                          }));
  },

  function getDefaultFontSizeOmitDetails() {
    fs.getDefaultFontSize(expect({
      pixelSize: 22,
      levelOfControl: CONTROLLED_BY_THIS_EXTENSION,
    }));
  },

  function getDefaultFixedFontSize() {
    fs.getDefaultFixedFontSize({}, expect({
                                 pixelSize: 42,
                                 levelOfControl: CONTROLLED_BY_THIS_EXTENSION,
                               }));
  },

  function getMinimumFontSize() {
    fs.getMinimumFontSize({}, expect({
                            pixelSize: 7,
                            levelOfControl: CONTROLLED_BY_THIS_EXTENSION,
                          }));
  },

  function clearPerScriptFont() {
    const script = 'Hang';
    const genericFamily = 'standard';
    const fontId = 'Tahoma';

    chrome.test.listenOnce(fs.onFontChanged, function(details) {
      chrome.test.assertEq(
          {
            script: script,
            genericFamily: genericFamily,
            fontId: fontId,
            levelOfControl: CONTROLLABLE_BY_THIS_EXTENSION,
          },
          details);
    });

    fs.clearFont(
        {
          script: script,
          genericFamily: genericFamily,
        },
        chrome.test.callbackPass());
  },

  function clearGlobalFont() {
    const script = 'Zyyy';
    const genericFamily = 'sansserif';
    const fontId = 'Arial';

    chrome.test.listenOnce(fs.onFontChanged, function(details) {
      chrome.test.assertEq(
          {
            script: script,
            genericFamily: genericFamily,
            fontId: fontId,
            levelOfControl: CONTROLLABLE_BY_THIS_EXTENSION,
          },
          details);
    });

    fs.clearFont(
        {
          genericFamily: genericFamily,
        },
        chrome.test.callbackPass());
  },

  function clearDefaultFontSize() {
    const pixelSize = 16;
    chrome.test.listenOnce(fs.onDefaultFontSizeChanged, function(details) {
      chrome.test.assertEq(
          {
            pixelSize: pixelSize,
            levelOfControl: CONTROLLABLE_BY_THIS_EXTENSION,
          },
          details);
    });

    fs.clearDefaultFontSize({}, chrome.test.callbackPass());
  },

  function clearDefaultFixedFontSize() {
    const pixelSize = 14;
    chrome.test.listenOnce(fs.onDefaultFixedFontSizeChanged, function(details) {
      chrome.test.assertEq(
          {
            pixelSize: pixelSize,
            levelOfControl: CONTROLLABLE_BY_THIS_EXTENSION,
          },
          details);
    });

    fs.clearDefaultFixedFontSize({}, chrome.test.callbackPass());
  },

  function clearMinimumFontSize() {
    const pixelSize = 8;
    chrome.test.listenOnce(fs.onMinimumFontSizeChanged, function(details) {
      chrome.test.assertEq(
          {
            pixelSize: pixelSize,
            levelOfControl: CONTROLLABLE_BY_THIS_EXTENSION,
          },
          details);
    });

    fs.clearMinimumFontSize({}, chrome.test.callbackPass());
  },
]);

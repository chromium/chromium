// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;

function getTestFunctionFor(keys, fails) {
  return function generatedTest () {
    // Debug.
    console.warn('keys: ' + keys + '; fails: ' + fails);

    chrome.chromeosInfoPrivate.get(
        keys,
        pass(
          function(values) {
            for (var i = 0; i < keys.length; ++i) {
              // Default session type should be normal.
              if (keys[i] == 'sessionType') {
                chrome.test.assertEq('normal', values[keys[i]]);
              }
              // PlayStoreStatus by default should be not available.
              if (keys[i] == 'playStoreStatus') {
                chrome.test.assertEq('not available', values[keys[i]]);
              }
              if (keys[i] == 'managedDeviceStatus') {
                chrome.test.assertEq('not managed', values[keys[i]]);
              }
              // Debug
              if (keys[i] in values) {
                console.log('  values["' + keys[i] + '"] = ' +
                            values[keys[i]]);
              } else {
                console.log('  ' + keys[i] + ' is missing in values');
              }

              chrome.test.assertEq(fails.indexOf(keys[i]) == -1,
                                   keys[i] in values);
            }
          }
        )
    );
  }
}

// Automatically generates tests for the given possible keys. Note, this
// tests do not check return value, only the fact that it is presented.
function generateTestsForKeys(keys) {
  var tests = [];
  // Test with all the keys at one.
  tests.push(getTestFunctionFor(keys, []));
  // Tests with key which hasn't corresponding value.
  var noValueKey = 'noValueForThisKey';
  tests.push(getTestFunctionFor([noValueKey], [noValueKey]));

  if (keys.length > 1) {
    // Tests with the separate keys.
    for (var i = 0; i < keys.length; ++i) {
      tests.push(getTestFunctionFor([keys[i]], []));
    }
  }
  if (keys.length >= 2) {
    tests.push(getTestFunctionFor([keys[0], keys[1]], []));
    tests.push(getTestFunctionFor([keys[0], noValueKey, keys[1]],
                                  [noValueKey]));
  }
  return tests;
}

function timezoneSetTest() {
  chrome.chromeosInfoPrivate.set('timezone', 'Pacific/Kiritimati');
  chrome.chromeosInfoPrivate.get(
      ['timezone'],
      pass(
        function(values) {
          chrome.test.assertEq(values['timezone'],
                               'Pacific/Kiritimati');
        }
      ));
}

function prefsTest(prefs) {
  for (const pref of prefs) {
    chrome.chromeosInfoPrivate.set(pref, true);
  }
  chrome.chromeosInfoPrivate.get(prefs, pass(function(values) {
                                   for (const pref of prefs) {
                                     chrome.test.assertEq(values[pref], true);
                                   }
                                 }));
}

function runningOnLacrosTest(isLacros) {
  chrome.chromeosInfoPrivate.isRunningOnLacros(pass(function(runningOnLacros) {
    chrome.test.assertEq(!!isLacros, !!runningOnLacros);
  }));
}

chrome.test.getConfig(function(config) {
  var tests = [];
  switch (config.customArg) {
    case 'dockedMagnifier':
      tests.push(() => prefsTest(['a11yDockedMagnifierEnabled']));
      break;
    case 'screenMagnifier':
      tests.push(() => prefsTest(['a11yScreenMagnifierEnabled']));
      break;
    case 'isRunningOnLacros - False':
      tests.push(() => runningOnLacrosTest(false));
      break;
    case 'isRunningOnLacros - True':
      tests.push(() => runningOnLacrosTest(true));
      break;
    default:
      // Generated chrome.chromeosInfoPrivate.get() tests.
      tests = generateTestsForKeys([
        'hwid',
        'deviceRequisition',
        'isMeetDevice',
        'customizationId',
        'homeProvider',
        'initialLocale',
        'board',
        'isOwner',
        'sessionType',
        'playStoreStatus',
        'managedDeviceStatus',
        'clientId',
        'a11yLargeCursorEnabled',
        'a11yStickyKeysEnabled',
        'a11ySpokenFeedbackEnabled',
        'a11yHighContrastEnabled',
        'a11yScreenMagnifierEnabled',
        'a11yAutoClickEnabled',
        'a11yVirtualKeyboardEnabled',
        'a11yCaretHighlightEnabled',
        'a11yCursorColorEnabled',
        'a11yFocusHighlightEnabled',
        'a11ySelectToSpeakEnabled',
        'a11yCursorColorEnabled',
        'a11ySwitchAccessEnabled',
        'a11yDockedMagnifierEnabled',
        'sendFunctionKeys',
        'timezone',
        'supportedTimezones'
      ]);

      // Add chrome.chromeosInfoPrivate.set() test.
      tests.push(timezoneSetTest);
      tests.push(() => prefsTest([
                   'a11yLargeCursorEnabled', 'a11yStickyKeysEnabled',
                   'a11ySpokenFeedbackEnabled', 'a11yHighContrastEnabled',
                   'a11yAutoClickEnabled', 'a11yVirtualKeyboardEnabled',
                   'a11yCaretHighlightEnabled', 'a11yCursorHighlightEnabled',
                   'a11yFocusHighlightEnabled', 'a11ySelectToSpeakEnabled',
                   'a11ySwitchAccessEnabled', 'a11yCursorColorEnabled',
                   'sendFunctionKeys'
                 ]));
      break;
  }
  chrome.test.runTests(tests);
});

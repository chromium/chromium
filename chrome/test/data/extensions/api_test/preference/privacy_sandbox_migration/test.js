// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Privacy Sandbox Migration API test
// Run with browser_tests
// --gtest_filter=ExtensionPreferenceApiTest.PrivacySandboxMigration

var privacyWebsitesNamespace = chrome.privacy.websites;

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

// Verifies that the preference has the expected default value.
function expectDefault(prefName, defaultValue) {
  return expect(
      {value: defaultValue, levelOfControl: 'controllable_by_this_extension'},
      '`' + prefName + '` is expected to be the default, which is ' +
          defaultValue);
}

// Verifies that the preference is properly controlled by the extension.
function expectControlled(prefName, newValue) {
  return expect(
      {
        value: newValue,
        levelOfControl: 'controlled_by_this_extension',
      },
      '`' + prefName + '` is expected to be controlled by this extension');
}

function setToFalsePref() {
  chrome.test.getConfig(function(config) {
    chrome.test.runTests([
      // Setting the deprecated pref to false |kPrivacySandboxApisEnabled|
      // should also set to false the new Privacy Sandbox APIs in order to
      // maintain backward compatibility during the migration period.
      function testSetToFalsePrivacyGuideEnabled() {
        privacyWebsitesNamespace.privacySandboxEnabled.set(
            {value: false}, function() {
              privacyWebsitesNamespace.topicsEnabled.get(
                  {}, expectControlled('topicsEnabled', false));
              privacyWebsitesNamespace.fledgeEnabled.get(
                  {}, expectControlled('fledgeEnabled', false));
              privacyWebsitesNamespace.adMeasurementEnabled.get(
                  {}, expectControlled('adMeasurementEnabled', false));
            });
      },
    ])
  })
}

function setToTruePref() {
  chrome.test.getConfig(function(config) {
    chrome.test.runTests([
      // Setting the deprecated pref to true |kPrivacySandboxApisEnabled|
      // should clear the new Privacy Sandbox APIs in order to
      // maintain backward compatibility during the migration period.
      function testSetToTruePrivacyGuideEnabled() {
        privacyWebsitesNamespace.privacySandboxEnabled.set(
            {value: true}, function() {
              privacyWebsitesNamespace.topicsEnabled.get(
                  {}, expectDefault('topicsEnabled', true));
              privacyWebsitesNamespace.fledgeEnabled.get(
                  {}, expectDefault('fledgeEnabled', true));
              privacyWebsitesNamespace.adMeasurementEnabled.get(
                  {}, expectDefault('adMeasurementEnabled', true));
            });
      },
    ])
  })
}

function clearPref() {
  chrome.test.getConfig(function(config) {
    chrome.test.runTests([
      // Clearing the deprecated pref|kPrivacySandboxApisEnabled|
      // should also clear the new k-APIs in order to maintain backward
      // compatibility during the migration period.
      function testClearPrivacyGuideEnabled() {
        privacyWebsitesNamespace.privacySandboxEnabled.clear({}, function() {
          privacyWebsitesNamespace.topicsEnabled.get(
              {}, expectDefault('topicsEnabled', true));
          privacyWebsitesNamespace.fledgeEnabled.get(
              {}, expectDefault('fledgeEnabled', true));
          privacyWebsitesNamespace.adMeasurementEnabled.get(
              {}, expectDefault('adMeasurementEnabled', true));
        });
      },
    ])
  })
}

chrome.test.sendMessage('ready', function(message) {
  if (message == 'run set to false test') {
    setToFalsePref();
  } else if (message == 'run set to true test') {
    setToTruePref();
  } else if (message == 'run clear test') {
    clearPref();
  }
});

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Privacy Sandbox Migration API test
// Run with browser_tests
// --gtest_filter=ExtensionPreferenceApiTest.PrivacySandboxMigration

// Verifies that the preference has the expected default value.
function expectDefault(prefName, defaultValue, realValue) {
  chrome.test.assertEq(
      {value: defaultValue, levelOfControl: 'controllable_by_this_extension'},
      realValue,
      '`' + prefName + '` is expected to be the default, which is ' +
          defaultValue);
}

// Verifies that the preference is properly controlled by the extension.
function expectControlled(prefName, newValue, realValue) {
  chrome.test.assertEq(
      {
        value: newValue,
        levelOfControl: 'controlled_by_this_extension',
      },
      realValue,
      '`' + prefName + '` is expected to be controlled by this extension');
}

async function setDeprecatedPrefToFalseAndVerifyNewApisAreDisabled() {
  await chrome.privacy.websites.privacySandboxEnabled.set({value: false});

  // Waits for C++ checks.
  await chrome.test.sendMessage('disable end');

  // Checks that the new APIs are cleared.
  const resultTopics = await chrome.privacy.websites.topicsEnabled.get({});
  expectControlled('topicsEnabled', false, resultTopics)
  const resultFledge = await chrome.privacy.websites.fledgeEnabled.get({});
  expectControlled('fledgeEnabled', false, resultFledge)
  const resultAdMeasurement =
      await chrome.privacy.websites.adMeasurementEnabled.get({});
  expectControlled('adMeasurementEnabled', false, resultAdMeasurement);

  chrome.test.succeed();
}

async function setDeprecatedPrefToTrueAndVerifyNewApisAreCleared() {
  await chrome.privacy.websites.privacySandboxEnabled.set({value: true});

  // Waits for C++ checks.
  await chrome.test.sendMessage('enable end');

  // Checks that the new APIs are cleared (they aren't enabled because we don't
  // allow extensions to enable the new APIs directly).
  const resultTopics = await chrome.privacy.websites.topicsEnabled.get({});
  expectDefault('topicsEnabled', true, resultTopics)
  const resultFledge = await chrome.privacy.websites.fledgeEnabled.get({});
  expectDefault('fledgeEnabled', true, resultFledge)
  const resultAdMeasurement =
      await chrome.privacy.websites.adMeasurementEnabled.get({});
  expectDefault('adMeasurementEnabled', true, resultAdMeasurement);

  chrome.test.succeed();
}

async function setToFalsePrivacySandboxEnabledSecond() {
  await chrome.privacy.websites.privacySandboxEnabled.set({value: false});
  // Waits for C++ checks.
  await chrome.test.sendMessage('disable end second');
  chrome.test.succeed();
}

async function clearDeprecatedPrefAndVerifyNewApisAreCleared() {
  await chrome.privacy.websites.privacySandboxEnabled.clear({});

  // Waits for C++ checks.
  await chrome.test.sendMessage('clear end');

  // Checks that the new APIs are also cleared.
  const resultTopics = await chrome.privacy.websites.topicsEnabled.get({});
  expectDefault('topicsEnabled', true, resultTopics)
  const resultFledge = await chrome.privacy.websites.fledgeEnabled.get({});
  expectDefault('fledgeEnabled', true, resultFledge)
  const resultAdMeasurement =
      await chrome.privacy.websites.adMeasurementEnabled.get({});
  expectDefault('adMeasurementEnabled', true, resultAdMeasurement);

  chrome.test.succeed();
}

async function setDeprecatedPrefToFalseOutsideTest() {
  await chrome.privacy.websites.privacySandboxEnabled.set({value: false});
  chrome.test.sendMessage('disable no test end');
}

chrome.test.sendMessage('ready', (message) => {
  if (message == 'run tests') {
    chrome.test.runTests([
      setDeprecatedPrefToFalseAndVerifyNewApisAreDisabled,
      setDeprecatedPrefToTrueAndVerifyNewApisAreCleared,
      setToFalsePrivacySandboxEnabledSecond,
      clearDeprecatedPrefAndVerifyNewApisAreCleared,
    ]);
  }

  if (message == 'disable no test') {
    setDeprecatedPrefToFalseOutsideTest();
  }
});

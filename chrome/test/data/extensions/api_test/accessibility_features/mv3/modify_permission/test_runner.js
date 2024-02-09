// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Tests that the extension is not able to get feature state.
 * @param {string} feature The feature to query.
 */
function testGetFeatureNotAllowed(feature) {
  var expectedFailure = 'You do not have permission to access the preference ' +
      '\'' + feature + '\'. Be sure to declare in your manifest what ' +
      'permissions you need.';
  chrome.accessibilityFeatures[feature].get(
      {}, chrome.test.callbackFail(expectedFailure));
}

/**
 * Initializes and runs tests that verify the extension cannot query feature
 * states.
 * @param {Array<string>} enabledFeatures The list of enabled features.
 * @param {Array<string>} disabledFeatures The list of disabled features.
 */
function runGetterTest(enabledFeatures, disabledFeatures) {
  var tests = [];

  enabledFeatures.forEach((feature) => {
    var test = testGetFeatureNotAllowed.bind(null, feature);
    // This is the name that will show up in the apitest framework's logging
    // output for anonymous functions.
    test.generatedName = 'testIsEnabledNotAllowed_' + feature;

    tests.push(test);
  });

  disabledFeatures.forEach((feature) => {
    var test = testGetFeatureNotAllowed.bind(null, feature);
    // This is the name that will show up in the apitest framework's logging
    // output for anonymous functions.
    test.generatedName = 'testIsDisabledNotAllowed_' + feature;

    tests.push(test);
  });

  chrome.test.runTests(tests);
}

/**
 * Sets the feature value and tests the set call succeeds.
 * @param {string} feature The feature that should be set.
 * @param {boolean} value The value to which the feature should be set.
 */
function testEnableFeature(feature, value) {
  chrome.accessibilityFeatures[feature].set(
      {value: value}, chrome.test.callbackPass(() => {}));
}

/**
 * Initializes and runs tests that verify that the extension is able to use
 * features' set method. The tests try to flip feature states and verify that
 * the setter methods do not cause API errors. The tests don't verify that the
 * feature state actually changes as a result of setter calls. That should be
 * done in Chrome part of test, after the test extension is done.
 * @param {Array<string>} initiallyEnabledFeatures The list of features that
 *     are enabled when the test starts.
 * @param {Array<string>} initiallyDisabledFeatures The list of features that
 *     are disabled when the test starts.
 */
function runSetterTest(initiallyEnabledFeatures, initiallyDisabledFeatures) {
  var tests = [];

  initiallyEnabledFeatures.forEach((feature) => {
    var test = testEnableFeature.bind(null, feature, false);
    // This is the name that will show up in the apitest framework's logging
    // output for anonymous functions.
    test.generatedName = 'testDisable_' + feature;

    tests.push(test);
  });

  initiallyDisabledFeatures.forEach((feature) => {
    var test = testEnableFeature.bind(null, feature, true);
    // This is the name that will show up in the apitest framework's logging
    // output for anonymous functions.
    test.generatedName = 'testEnable_' + feature;

    tests.push(test);
  });

  chrome.test.runTests(tests);
}

/**
 * Mapping from test name to the function that runs tests.
 * @type {getterTest: function(Array<string>, Array<string>),
 *        setterTest: function(Array<string>, Array<string>)}
 * @const
 */
var TEST_FUNCTIONS = {'getterTest': runGetterTest, 'setterTest': runSetterTest};

/**
 * Entry point for tests. Gets test config and runs the associated test
 * function.
 */
chrome.test.getConfig((config) => {
  var testArgs = JSON.parse(config.customArg);
  if (!testArgs) {
    chrome.test.notifyFail('No test args');
    return;
  }
  if (!testArgs.testName) {
    chrome.test.notifyFail('No test name');
    return;
  }

  if (!TEST_FUNCTIONS[testArgs.testName]) {
    chrome.test.notifyFail('Unknown test name');
    return;
  }

  TEST_FUNCTIONS[testArgs.testName](testArgs.enabled, testArgs.disabled);
});

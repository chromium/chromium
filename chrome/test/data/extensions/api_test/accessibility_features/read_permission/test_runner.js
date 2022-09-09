// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Tests that the extension is able to query feature status and that the feature
 * status is as expected.
 * @param {string} featureName The boolean feature to be queried.
 * @param {boolean} expectedValue The expected value of the feature.
 */
function testFeatureIsEnabled(featureName, expectedIsEnabled) {
  chrome.accessibilityFeatures[featureName].get(
      {},
      function(result) {
        chrome.test.assertTrue(!!result);
        chrome.test.assertEq(expectedIsEnabled,
                             result.value,
                             "Unexpected value for feature " + featureName);
        chrome.test.succeed();
      });
};

/**
 * Initializes and runs tests that get feature statuses.
 * @param {Array<string>} enabledFeatures The list of features that are
 *     expected to be enabled.
 * @param {Array<string>} disabledFeatures The list of features that are
 *     expected to be disabled.
 */
function runGetterTest(enabledFeatures, disabledFeatures) {
  var tests= [];

  enabledFeatures.forEach(function(feature) {
    var test = testFeatureIsEnabled.bind(null, feature, true);
    // This is the name that will show up in the apitest framework's logging
    // output for anonymous functions.
    test.generatedName = "testIsEnabled_" + feature;

    tests.push(test);
  });

  disabledFeatures.forEach(function(feature) {
    var test = testFeatureIsEnabled.bind(null, feature, false);
    // This is the name that will show up in the apitest framework's logging
    // output for anonymous functions.
    test.generatedName = "testIsDisabled_" + feature;

    tests.push(test);
  });

  chrome.test.runTests(tests);
};

/**
 * Tests that the extension is not able to modify a feature value.
 * @param {string} feature The feature to be modified.
 * @param {boolean} value The value the feature should be set to.
 */
function testSetFeatureNotAllowed(feature, value) {
  var expectedError = 'You do not have permission to access the preference ' +
      '\'' + feature + '\'. Be sure to declare in your manifest what ' +
      'permissions you need.';

  chrome.accessibilityFeatures[feature].set(
      {value: value},
      chrome.test.callbackFail(expectedError));
}

/**
 * Initializes and runs tests that verify the extension cannot modify features.
 * In the test the extension tries to flip the feature values.
 * Note that the tests only verify that set method fails. They don't verify the
 * feature values remain unchanged. That is done by the Chrome side of the test.
 * @param {Array<string>} enabledFeatures The list of features that are
 *     enabled at the start of the test.
 * @param {Array<string>} disabledFeatures The list of features that are
 *     disabled at the start of the test.
 */
function runSetterTest(enabledFeatures, disabledFeatures) {
  var tests = [];

  enabledFeatures.forEach(function(feature) {
    var test = testSetFeatureNotAllowed.bind(null, feature, false);
    // This is the name that will show up in the apitest framework's logging
    // output for anonymous functions.
    test.generatedName = "testDisableNotAllowed_" + feature;

    tests.push(test);
  });

  disabledFeatures.forEach(function(feature) {
    var test = testSetFeatureNotAllowed.bind(null, feature, true);
    // This is the name that will show up in the apitest framework's logging
    // output for anonymous functions.
    test.generatedName = "testEnableNotAllowed_" + feature;

    tests.push(test);
  });

  chrome.test.runTests(tests);
}

/**
 * Status of the observer test Set if an observer test is running. Contains
 * two lists of feature for which an {@code onChange} event is expected to be
 * observed. One list contains features for which the event value should be true
 * ({@code toBeEnabled}), while the other one contains features for which the
 * event value is expected to be false ({@code toBeDisabled}).
 * Once an event for a feature is observed, the feature is removed from the list
 * that contains it.
 * @type {?{toBeDisabled: Array<{name: string,
 *                                listener: function(Event)}>,
 *          toBeEnabled: Array<{name: string,
 *                               listener: function(Event)}>}}
 *     For each array type: |name|: the feature name;
 *                          |listener|: {@code onChange} listener.
 */
var observerTestState = null;

/**
 * Initializes and starts the test that observes that {@code onChange} event is
 * triggered when a feature status changes. The features are actually changed in
 * Chrome side of the test. During a single test, two success notification will
 * be sent (if the test succeeds): once when the features' change listeners have
 * been setup, and the second time when all the expected events are seen.
 * In case of the failure notification, no further notifications will be sent.
 * @param {Array<string>} initiallyEnabled
 *     The list of features that are enabled at the start of the test, and for
 *     which a {@code onChange} event with value false should be observed.
 * @param {Array<string>} initiallyDisabled
 *     The list of features that are disabled at the start of the test, and for
 *     which a {@code onChange} event with value true should be observed.
 */
function startObserverTest(initiallyEnabled, initiallyDisabled) {
  if (observerTestState) {
    chrome.test.notifyFail('Initializing observe features test before the ' +
                           'previous one finished');
    return;
  }

  observerTestState = {
    toBeDisabled: [],
    toBeEnabled: []
  };

  /**
   * Finds feature with the provided name in the.
   * @param {Array<{name: string, listener: function(Event)>} list The list in
   *     which to search.
   * @param {string} featureName The feature name for which to search.
   * @return {number} The feature's index in the list. If not found returns -1.
   */
  function findFeatureIndex(list, featureName) {
    for (var i = 0; i < list.length; ++i)
      if (list[i].name == featureName)
        return i;
    return -1;
  }

  /**
   * Initializes listener for a feature and adds it to the appropriate list in
   * {@code observerTestState}.
   * @param {string} feature The feature name.
   * @param {boolean} initiallyEnabled Whether the feature is initially enabled.
   */
  function initTestParamsForFeature(feature, initiallyEnabled) {
    var list = initiallyEnabled ? observerTestState.toBeDisabled :
                                  observerTestState.toBeEnabled;

    var listener = function(ev) {
      // Fail the test in case the new feature value is not as expected, but
      // before that, do some cleanup.
      if (initiallyEnabled == ev.value)
        clearRemainingListeners();
      chrome.test.assertEq(!initiallyEnabled, ev.value);

      var index = findFeatureIndex(list, feature);
      if (index < 0)
        clearRemaningListeners();
      chrome.test.assertTrue(index > -1);

      chrome.accessibilityFeatures[feature].onChange.removeListener(
          list[index].listener);
      list.splice(index, 1);

      if (observerTestState.toBeEnabled.length == 0 &&
          observerTestState.toBeDisabled.length == 0) {
        chrome.test.succeed();
      }
    }

    list.push({name: feature, listener: listener});
    chrome.accessibilityFeatures[feature].onChange.addListener(listener);
  }

  /**
   * If any features are still being disabled, removes their listeners.
   * This situation may happen when test fails.
   */
  function clearRemainingListeners() {
    observerTestState.toBeEnabled.forEach(function(feature) {
      chrome.accessibilityFeatures[feature.name].onChange.removeListener(
          feature.listener);
      feature.listener = null;
    });
    observerTestState.toBeDisabled.forEach(function(feature) {
      chrome.accessibilityFeatures[feature.name].onChange.removeListener(
          feature.listener);
      feature.listener = null;
    });
  }

  initiallyEnabled.forEach(function(feature) {
    initTestParamsForFeature(feature, true);
  });

  initiallyDisabled.forEach(function(feature) {
    initTestParamsForFeature(feature, false);
  });

  // Send success, so the Chrome side of the test continues. The test is not
  // actually over yet, and is expected to send another success notification
  // when all featureChanged events are seen.
  chrome.test.succeed();
}

/**
 * Mapping from test name to the function that runs tests.
 * @type {getterTest: function(Array<string>, Array<string>),
 *        setterTest: function(Array<string>, Array<string>),
 *        observerTest: function(Array<string>, Array<string>)}
 * @const
 */
var TEST_FUNCTIONS = {
  getterTest: runGetterTest,
  setterTest: runSetterTest,
  observerTest: startObserverTest,
};

/**
 * Entry point for tests. Gets test config and runs the associated test
 * function.
 */
chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.getConfig(function(config) {
    var testArgs = JSON.parse(config.customArg);
    if (!testArgs) {
      chrome.test.notifyFail("No test args");
      return;
    }
    if (!testArgs.testName) {
      chrome.test.notifyFail("No test name");
      return;
    }

    if (!TEST_FUNCTIONS[testArgs.testName]) {
      chrome.test.notifyFail("Unknown test name");
      return;
    }

    TEST_FUNCTIONS[testArgs.testName](testArgs.enabled, testArgs.disabled);
  });
});

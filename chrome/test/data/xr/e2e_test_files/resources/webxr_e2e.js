// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testPassed = false;
var resultString = "";
var javascriptDone = false;
var initializationSteps = {load: false};
var wouldPrompt = null;
var progressMessages = {};
const SINGLE_TEST_NAME = '_single_test_';

// Sets a status message for the test. This message will be shown
// alongside the test result when the test finishes. It is intended
// to provide some context about what the test state, including
// debugging information, without cluttering up the log.
function updateTestProgressMessage(test, message) {
  const name = test && test.name ? test.name : SINGLE_TEST_NAME;
  progressMessages[name] = message;
}

function updateSingleTestProgressMessage(message) {
  updateTestProgressMessage(null, message);
}

function finishJavaScriptStep() {
  if (javascriptDone) {
    testPassed = false;
    resultString = "Attempted to end a JavaScript step before Java/C++ acked a "
                    + "previous one.";
  }
  javascriptDone = true;
}

function checkPermissionRequestWouldTriggerPrompt(permissionName) {
  wouldPrompt = null;
  navigator.permissions.query({ name: permissionName }).then( (p) => {
    wouldPrompt = p.state == 'prompt';
  }, (err) => {
    throw 'Permission query rejected: ' + err;
  });
}

// Used to check when JavaScript is in an acceptable state to start testing
// after a page load, as Chrome thinking that the page has finished loading
// is not always sufficient. By default waits until the load event is fired.
function isInitializationComplete() {
  for (var step in initializationSteps) {
    if (!initializationSteps[step]) {
      return false;
    }
  }
  return true;
}

window.addEventListener("load",
    () => {initializationSteps["load"] = true;}, false);

function checkResultsForFailures(tests, harness_status) {
  testPassed = true;
  if (harness_status["status"] != 0) {
    testPassed = false;
    resultString += "Harness failed due to " +
                    (harness_status["status"] == 1 ? "error" : "timeout")
                    + ". ";
  }
  for (var testNum in tests) {
    let test = tests[testNum];
    let passed = (test["status"] == 0);
    if (!passed) {
      testPassed = false;
      resultString += "FAIL ";
    } else {
      resultString += "PASS ";
    }
    let message = test["message"];
    let progress =
        progressMessages[test.name] || progressMessages[SINGLE_TEST_NAME];
    resultString += test["name"] +
                    (passed ? "" : (": " + message)) +
                    (progress ? " [" + progress + "]" : "") +
                    ". ";
  }
}

// Only interact with testharness.js if it was actually included on the page
// before this file
if (typeof add_completion_callback !== "undefined") {
  add_completion_callback( (tests, harness_status) => {
    checkResultsForFailures(tests, harness_status);
    console.debug("Test result: " + (testPassed ? "Pass" : "Fail"));
    console.debug("Test result string: " + resultString);
    finishJavaScriptStep();
  });
}

if (typeof setup !== "undefined") {
  // The timeout multiplier helps with erroneous timeouts on slower Android
  // devices. 3 was chosen because it's a midpoint between the standard 10 and
  // long 60 second timeouts that are used for web tests using testharness.js
  // and it's a bit more of a multiplier than the 2x increase in step timeouts
  // we have for slow devices, which is reasonable since this is the timeout for
  // the entire test rather than a single step.
  setup({single_test: true, timeout_multiplier: 3});
}

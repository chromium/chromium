// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testPassed = false;
var resultString = "";
var javascriptDone = false;
var initializationSteps = {load: false};
var wouldPrompt = null;

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
  for (var test in tests) {
    let passed = (tests[test]["status"] == 0);
    if (!passed) {
      testPassed = false;
      resultString += "FAIL ";
    } else {
      resultString += "PASS ";
    }
    resultString += tests[test]["name"] +
                    (passed ? "" : (": " + tests[test]["message"])) +
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

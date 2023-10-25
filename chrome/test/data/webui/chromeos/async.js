// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Fails always.
 * @throw {Error}
 */
function testFailsAssert() {
  assertNotReached();
  chrome.send('testContinues');
  assertNotReached();
}

/**
 * Passes and sends testDone message for browser_test to call
 * testDone().
 */
function testPasses() {
  assertTrue(true);
  chrome.send('testContinues');
  assertFalse(false);
}

/**
 * Wraps the function represented by |name| similar to the way net_internals
 * tests are wrapped.
 * @param {string} name The name of the function to run.
 */
function runAsync(name) {
  // Strip |name| from arguments.
  const testArguments = Array.prototype.slice.call(arguments, 1);

  // call async function.
  const result = runTestFunction(name, this[name], testArguments);

  // Pass on success; bail on errors.
  if (result[0]) {
    chrome.send('testPasses');
  } else {
    chrome.send('testFails');
    testDone(result);
  }
}

/**
 * Sends a message to handler to start |testName| and returns.
 * @param {string} testName The name of the test to run.
 */
function startAsyncTest(testName) {
  chrome.send('startAsyncTest', [testName]);
}

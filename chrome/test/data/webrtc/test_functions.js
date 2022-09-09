/**
 * Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Helper / error handling functions.

/**
 * Prints a debug message.
 */
function debug(txt) {
  console.log(txt);
}

/**
 * Sends a value back to the test without logging it.
 *
 * @param {string} message The message to return.
 */
function silentReturnToTest(message) {
  window.domAutomationController.send(message);
}

/**
 * Sends a value back to the test and logs it.
 *
 * @param {string} message The message to return.
 */
function returnToTest(message) {
  debug('Returning ' + message + ' to test.');
  silentReturnToTest(message);
}


/**
 * Fails the test by generating an exception. If the test automation is calling
 * into us, make sure to fail the test as fast as possible. You must use this
 * function like this:
 *
 * throw failTest('my reason');
 *
 * @return {!Error}
 */
function failTest(reason) {
  var error = new Error(reason);
  returnToTest('Test failed: ' + error.stack);
  return error;
}

function success(method) {
  debug(method + '(): success.');
}

function failure(method, error) {
  throw failTest(method + '() failed: ' + JSON.stringify(error));
}

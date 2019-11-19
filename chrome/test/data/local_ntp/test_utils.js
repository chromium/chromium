// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test utilities for JavaScript tests of the local NTP.
 */

/**
 * Object for individual "module" objects, that then contain tests.
 */
var test = {};

/**
 * Shortcut for document.getElementById.
 * @param {string} id of the element.
 * @return {HTMLElement} with the id.
 */
function $(id) {
  return document.getElementById(id);
}

/**
 * Sets up the page for the next test case. Recreates a part of
 * the default local NTP DOM (test specific, based on the template).
 * @param {!string} templateId ID of the element to clone into the body element.
 */
function setUpPage(templateId) {
  // First, clear up the DOM and state left over from any previous test case.
  document.body.innerHTML = '';
  // The NTP stores some state such as fakebox focus in the body's classList.
  document.body.classList = '';

  document.body.appendChild($(templateId).content.cloneNode(true));
}

/**
 * Aborts a test if |got| !== true.
 * @param {boolean} got The boolean that must be true.
 * @param {string=} opt_message A message to log if the condition is not met.
 */
function assertTrue(got, opt_message) {
  if (got !== true) {
    throw new Error(opt_message || `Expected: 'true', got: 'false'`);
  }
}

/**
 * Aborts a test if |got| !== false.
 * @param {boolean} got The boolean that must be false.
 * @param {string=} opt_message A message to log if the condition is not met.
 */
function assertFalse(got, opt_message) {
  if (got !== false) {
    throw new Error(opt_message || `Expected: 'false', got: 'true'`);
  }
}

/**
 * Aborts a test if |expected| !== |got|.
 * @param {object} expected The expected value.
 * @param {object} got The obtained value.
 * @param {string=} opt_message A message to log if the paremeters
 *    are not equal.
 */
function assertEquals(expected, got, opt_message) {
  if (expected !== got) {
    throw new Error(opt_message || `Expected: '${expected}', got: '${got}'`);
  }
}

/**
 * Aborts a test if the |throws| function does not throw an error,
 * or the error message does not contain |expected|.
 * @param {!string} expected The expected error message substring.
 * @param {!Function} throws The function that should throw an error with
 *    a message containing |expected| as a substring.
 * @param {string=} opt_message A message to log if this assertion fails.
 */
function assertThrows(expected, throws, opt_message) {
  try {
    throws.call();
  } catch (err) {
    if (err.message.includes(expected)) {
      return;
    }
    throw new Error(opt_message || `Expected: '${expected}', got: '${err}'`);
  }
  throw new Error(opt_message || 'Got no error.');
}

/**
 * Calls |runTest()| on all simple tests within the |test[moduleName]| object
 * (any function in that object whose name starts with 'test').
 * @param {!string} moduleName Name of the module to test (used as
 *     an index into |test|).
 * @return {boolean} True if all tests pass and false otherwise.
 */
function runSimpleTests(moduleName) {
  let totalTests = 0;
  let passedTests = 0;
  const testModule = window.test[moduleName];
  if (!testModule) {
    throw new Error('Failed to find tests for module: ' + moduleName);
  }

  for (let testName in testModule) {
    if (/^test.+/.test(testName) && typeof testModule[testName] == 'function') {
      totalTests++;
      try {
        if (runTest(testModule, testName)) {
          passedTests++;
        }
      } catch (err) {
        console.log('Error occurred while running tests, aborting.');
        return false;
      }
    }
  }
  window.console.log(`Passed tests: ${passedTests}/${totalTests}`);
  return passedTests == totalTests;
}

/**
 * Sets up the environment using the |testModule.setUp()| function
 * if it exists, and calls the function under test, |testModule[testName]|.
 * The setup function is assumed to be provided by the browser
 * test script. In case setup fails, the error is logged and another error
 * is raised, which will abort the test procedure in |runSimpleTests()|.
 * @param {!object} testModule The object that contains tests for a module
 *     (a property of |test|).
 * @param {!string} testName The name of the test function to run.
 * @return {boolean} True iff the test function succeeds (does not
 *     throw an error).
 */
function runTest(testModule, testName) {
  try {
    if (typeof testModule.setup === 'function') {
      throw 'use setUp instead of setup';
    }
    if (typeof testModule.setUp === 'function') {
      testModule.setUp();
    }
  } catch (err) {
    window.console.log(`Setup for '${testName}' failed: ${err}`);
    window.console.log('Stacktrace:\n' + err.stack);
    throw new Error('Setup failed, aborting tests.');
  }

  // Run the test function.
  try {
    testModule[testName].call(testModule);
  } catch (err) {
    window.console.log(`Test '${testName}' failed: ${err}`);
    window.console.log('Stacktrace:\n' + err.stack);
    return false;
  }
  return true;
}

/**
 * Checks whether a given HTMLElement exists and is visible.
 * @param {HTMLElement|undefined} elem An HTMLElement.
 * @return {boolean} True if the element exists and is visible.
 */
function elementIsVisible(elem) {
  return elem && elem.offsetWidth > 0 && elem.offsetHeight > 0 &&
      window.getComputedStyle(elem).visibility != 'hidden' &&
      window.getComputedStyle(elem).opacity > 0;
}

/**
 * Creates and initializes a LocalNTP object.
 * @param {boolean} isGooglePage Whether to make it a Google-branded NTP.
 * @return {!Object} The LocalNTP object.
 */
function initLocalNTP(isGooglePage) {
  configData.isGooglePage = isGooglePage;
  var localNTP = LocalNTP();
  localNTP.disableIframesAndVoiceSearchForTesting();
  localNTP.init();
  return localNTP;
}

// ***************************** HELPER CLASSES *****************************
// Helper classes used for mocking in tests.

/**
 * A property replacer utility class. Useful for substituting real functions
 * and properties with mocks and then reverting the changes. Inspired by
 * Closure's PropertyReplacer class.
 * @constructor
 */
function Replacer() {
  /**
   * A dictionary of temporary objects that contain a reference to
   * the property's parent object (|parent|), and the property's
   * original value (|value|).
   */
  this.old_ = {};
}

/**
 * Replaces the property |propertyName| of the object |parent| with the
 * |newValue|. Stores the old value of the property to enable resetting
 * in |reset()|.
 * @param {!object} parent The object containg the property |propertyName|.
 * @param {!string} propertyName The name of the property.
 * @param {object} newValue The new value of the property.
 */
Replacer.prototype.replace = function(parent, propertyName, newValue) {
  if (!(propertyName in parent)) {
    throw new Error(
        `Cannot replace missing property "${propertyName}" in parent.`);
  }
  this.old_[propertyName] = {};
  this.old_[propertyName].value = parent[propertyName];
  this.old_[propertyName].parent = parent;
  parent[propertyName] = newValue;
};

/**
 * Resets every property that was overriden by |replace()| to its original
 * value.
 */
Replacer.prototype.reset = function() {
  Object.entries(this.old_).forEach(([propertyName, saved]) => {
    saved.parent[propertyName] = saved.value;
  });
  this.old_ = {};
};

/**
 * Utility for testing code that uses |setTimeout()| and |clearTimeout()|.
 * Inspired by Closure's MockClock class.
 * @constructor
 */
function MockClock() {
  /**
   * The current time, in milliseconds.
   */
  this.time = 0;

  /**
   * Records the timeouts that have been set and not yet executed, as an array
   * of {callback, activationTime, id} objects, ordered by activationTime.
   */
  this.pendingTimeouts = [];

  /**
   * An internal counter for assigning timeout ids.
   */
  this.nextTimeoutId_ = 1;

  /**
   * Property replacer, used for replacing |setTimeout()| and |clearTimeout()|.
   */
  this.stubs_ = new Replacer();
}

/**
 * Sets the current time that sbox.getTime returns.
 * @param {!number} msec The time in milliseconds.
 */
MockClock.prototype.setTime = function(msec) {
  assertTrue(this.time <= msec);
  this.time = msec;
};

/**
 * Advances the current time by the specified number of milliseconds.
 * @param {!number} msec The length of time to advance the current time by.
 */
MockClock.prototype.advanceTime = function(msec) {
  assertTrue(msec >= 0);
  this.time += msec;
};

/**
 * Restores the |setTimeout()| and |clearTimeout()| functions back to their
 * original implementation.
 */
MockClock.prototype.reset = function() {
  this.time = 0;
  this.pendingTimeouts = [];
  this.nextTimeoutId_ = 1;
  this.stubs_.reset();
};

/**
 * Checks whether a timeout with this id has been set.
 * @param {!number} id The timeout id.
 * @return True iff a timeout with this id has been set and the timeout
 *    has not yet fired.
 */
MockClock.prototype.isTimeoutSet = function(id) {
  return 0 < id && id <= this.nextTimeoutId_ &&
      this.pendingTimeouts.map(t => t.id).includes(id);
};

/**
 * Substitutes the global |setTimeout()| and |clearTimeout()| functions
 * with their mocked variants, until |reset()| is called.
 */
MockClock.prototype.install = function() {
  this.stubs_.replace(window, 'clearTimeout', (id) => {
    if (!id) {
      return;
    }
    for (let i = 0; i < this.pendingTimeouts.length; ++i) {
      if (this.pendingTimeouts[i].id == id) {
        this.pendingTimeouts.splice(i, 1);
        return;
      }
    }
  });
  this.stubs_.replace(window, 'setTimeout', (callback, interval) => {
    const timeoutId = this.nextTimeoutId_++;
    this.pendingTimeouts.push({
      'callback': callback,
      'activationTime': this.time + interval,
      'id': timeoutId
    });
    this.pendingTimeouts.sort(function(a, b) {
      return a.activationTime - b.activationTime;
    });
    return timeoutId;
  });
};

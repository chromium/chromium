// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A mocha adapter for BrowserTests. To use, include mocha.js and
 * mocha_adapter.js in a WebUIBrowserTest's extraLibraries array.
 */

// NOTE: When defining TEST_F() functions that use Mocha, use 'var self = this'
// for referencing the Test object within suite() and test() function objects
// (instead of binding |this|), since |this| within those objects will reference
// the Mocha Suite or Test instance.

/**
 * Initializes a mocha reporter for the BrowserTest framework, which registers
 * event listeners on the given Runner.
 * @constructor
 * @param {Runner} runner Runs the tests and provides hooks for test results
 *     (see Runner.prototype in mocha.js).
 */
function BrowserTestReporter(runner) {
  var passes = 0;
  var failures = 0;

  // Increment passes for each passed test.
  runner.on('pass', function(test) {
    passes++;
  });

  // Report failures. Mocha only catches "assert" failures, because "expect"
  // failures are caught by test_api.js.
  runner.on('fail', function(test, err) {
    failures++;
    var message = 'Mocha test failed: ' + test.fullTitle() + '\n';

    // Remove unhelpful mocha lines from stack trace.
    if (err.stack) {
      var stack = err.stack.split('\n');
      for (var i = 0; i < stack.length; i++) {
        if (stack[i].indexOf('mocha.js:') === -1) {
          message += stack[i] + '\n';
        }
      }
    } else {
      message += err.toString();
    }

    console.error(message);
  });

  // Report the results to the test API.
  runner.on('end', function() {
    if (failures === 0) {
      if (passes > 0) {
        testDone();
      } else {
        testDone([false, 'Failure: Mocha ran, but no mocha tests were run!']);
      }
      return;
    }
    testDone([
      false,
      'Test Errors: ' + failures + '/' + (passes + failures) +
          ' tests had failed assertions.'
    ]);
  });
}

/**
 * Helper function provided to make running a single Mocha test more robust.
 * @param {string} suiteName
 * @param {string} testName
 */
window.runMochaTest = function(suiteName, testName) {
  const escapedTestName = testName.replace(/[|\\{}()[\]^$+*?.]/g, '\\$&');
  mocha.grep(new RegExp('^' + suiteName + ' ' + escapedTestName + '$')).run();
};

/**
 * Helper function provided to make running a single Mocha suite more robust.
 * @param {string} suiteName
 */
window.runMochaSuite = function(suiteName) {
  mocha.grep(new RegExp('^' + suiteName + ' ')).run();
};

// Configure mocha.
mocha.setup({
  // Use TDD interface instead of BDD.
  ui: 'tdd',
  // Use custom reporter to interface with BrowserTests.
  reporter: BrowserTestReporter,
  // Mocha timeouts are set to 2 seconds initially. This isn't nearly enough for
  // slower bots (e.g., Dr. Memory). Disable timeouts globally, because the C++
  // will handle it (and has scaled timeouts for slower bots).
  enableTimeouts: false,
});

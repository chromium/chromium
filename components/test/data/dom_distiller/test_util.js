// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let resolve;
let reject;
window.completePromise = new Promise((res, rej) => {
  resolve = res;
  reject = rej;
});

// Based on BrowserTestReporter() in //chrome/test/data/webui/mocha_adapter.js
// TODO(crbug.com/40108835): Look into using that class directly.
function TestReporter(runner) {
  let passes = 0;
  let failures = 0;

  runner.on('pass', function(test) {
    passes++;
  });

  // TODO(crbug.com/40108835): Show diff between actual and expected results.
  runner.on('fail', function(test, err) {
    failures++;
    let message = 'Mocha test failed: ' + test.fullTitle() + '\n';

    // Remove unhelpful mocha lines from stack trace.
    if (err.stack) {
      const stack = err.stack.split('\n');
      for (let i = 0; i < stack.length; i++) {
        if (stack[i].indexOf('mocha.js:') == -1) {
          message += stack[i] + '\n';
        }
      }
    } else {
      message += err.toString();
    }

    console.error(message);
  });

  runner.on('end', function() {
    if (failures === 0 && passes > 0) {
      return resolve();
    }
    return reject(new Error('Some tests failed, or no tests were run'));
  });
}

mocha.setup({
  ui: 'tdd',
  reporter: TestReporter,
  enableTimeouts: false,
});

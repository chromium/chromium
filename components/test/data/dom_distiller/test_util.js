// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on BrowserTestReporter() in //chrome/test/data/webui/mocha_adapter.js
// TODO(crbug.com/1027612): Look into using that class directly.
function TestReporter(runner) {
  let passes = 0;
  let failures = 0;

  runner.on('pass', function(test) {
    passes++;
  });

  // TODO(crbug.com/1027612): Show diff between actual and expected results.
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
    window.domAutomationController.send(failures === 0 && passes > 0);
  });
}

mocha.setup({
  ui: 'tdd',
  reporter: TestReporter,
  enableTimeouts: false,
});

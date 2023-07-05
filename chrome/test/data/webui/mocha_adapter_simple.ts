// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Mocha adapter for WebUI tests.
 *   1) Uses `window.domAutomationController` to signal test completion
 *      (success or error) back to the WebUIMochaBrowserTest class instance.
 *   2) Emits console messages as Mocha tests are making progress.
 *
 * To use, include mocha.js and mocha_adapter_simple.js along with the Mocha
 * test code.
 */

// Messages passed back to WebUIMochaBrowserTest C++ class.
enum TestStatus {
  FAILURE = 'FAILURE',
  PENDING = 'PENDING',
  SUCCESS = 'SUCCESS',
}

class WebUiMochaBrowserTestReporter extends Mocha.reporters.Base {
  private indents_: number = 0;

  constructor(runner: Mocha.Runner, options: Mocha.MochaOptions) {
    super(runner, options);

    const stats = runner.stats!;

    const constants = Mocha.Runner.constants;
    runner
        .once(
            constants.EVENT_RUN_BEGIN,
            () => {
              console.info('start');
            })
        .on(constants.EVENT_SUITE_BEGIN,
            () => {
              this.increaseIndent_();
            })
        .on(constants.EVENT_SUITE_END,
            () => {
              this.decreaseIndent_();
            })
        .on(constants.EVENT_TEST_BEGIN,
            test => {
              console.info(`${this.indent_()}started: ${test.fullTitle()}`);
              window.domAutomationController.send(TestStatus.PENDING);
            })
        .on(constants.EVENT_TEST_PASS,
            test => {
              console.info(`${this.indent_()} passed: ${test.fullTitle()}`);
            })
        .on(constants.EVENT_TEST_FAIL,
            (test, err) => {
              let message = `${this.indent_()} failed: ${test.fullTitle()}\n`;

              if (err.stack) {
                message += err.stack;
              } else {
                message += err.toString();
              }

              console.info(message);
            })
        .once(constants.EVENT_RUN_END, () => {
          console.info(
              `end: ${stats.passes}/${stats.passes + stats.failures} ok`);
          const success = stats.failures === 0 && stats.passes > 0;
          console.info(
              'TEST all complete, status=' + (success ? 'PASS' : 'FAIL') +
              ', duration=' + Math.round(stats.duration!) + 'ms');
          if (stats.failures + stats.passes === 0) {
            console.info(
                'No tests were found. Look for any uncaught errors that might ' +
                'have caused this');
          }
          window.domAutomationController.send(
              success ? TestStatus.SUCCESS : TestStatus.FAILURE);
        });
  }

  private indent_() {
    return Array(this.indents_).join('  ');
  }

  private increaseIndent_() {
    this.indents_++;
  }

  private decreaseIndent_() {
    this.indents_--;
  }
}

// Helper function provided to make running a single Mocha test more robust.
function runMochaTest(suiteName: string, testName: string) {
  const escapedTestName = testName.replace(/[|\\{}()[\]^$+*?.]/g, '\\$&');
  mocha.grep(new RegExp('^' + suiteName + ' ' + escapedTestName + '$')).run();
}

// Helper function provided to make running a single Mocha suite more robust.
function runMochaSuite(suiteName: string) {
  mocha.grep(new RegExp('^' + suiteName + ' ')).run();
}

Object.assign(window, {runMochaSuite, runMochaTest});

// Configure mocha.
mocha.setup({
  // Use TDD interface instead of BDD.
  ui: 'tdd',
  // Use custom reporter to interface with WebUIMochaBrowserTest C++ class.
  reporter: WebUiMochaBrowserTestReporter,
  // Mocha timeouts are set to 2 seconds initially. This isn't nearly enough for
  // slower bots (e.g., Dr. Memory). Disable timeouts globally, because the C++
  // will handle it (and has scaled timeouts for slower bots).
  timeout: '0',
});

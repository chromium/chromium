// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for Machine Learning Internals WebUI. This test uses
 * a fake machine learning service connection with the given expectedValue as
 * output.
 */

const ROOT_PATH = '../../../../../';
// Fixed graph execution result returned by the fake ML Service in these tests.
const expectedValue = '1234.4321';

GEN('#include "chrome/browser/ui/webui/chromeos/machine_learning/machine_learning_internals_browsertest.h"');

var MachineLearningInternalsWebUIBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return 'chrome://machine-learning-internals';
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @override */
  get runAccessibilityChecks() {
    return false;
  }

  /** @override */
  get typedefCppFixture() {
    return 'MachineLearningInternalsBrowserTest';
  }

  /** @override */
  testGenPreamble() {
    // Make the backend connect to a fake machine learning service, which simply
    // returns a Tensor containing the expectedValue as output.
    GEN(`SetupFakeConnectionAndOutput(${expectedValue});`);
  }

  /** @override */
  get extraLibraries() {
    return [
      ROOT_PATH + 'third_party/mocha/mocha.js',
      ROOT_PATH + 'chrome/test/data/webui/mocha_adapter.js',
    ];
  }
};

TEST_F('MachineLearningInternalsWebUIBrowserTest', 'All', function() {
  test('when X and Y are not numbers', async function() {
    $('test-input-x').value = 'a';
    $('test-input-y').value = 'b';
    await machine_learning_internals.testExecute();
    assertEquals(
        $('test-status').textContent, '"X" and "Y" should both be numbers');
  });

  test('when run on fake ml service', async function() {
    $('test-input-x').value = '1';
    $('test-input-y').value = '2';
    await machine_learning_internals.testExecute();
    assertEquals($('test-status').textContent, 'Execute Result is OK.');
    assertEquals($('test-output').textContent, expectedValue);
  });

  mocha.run();
});

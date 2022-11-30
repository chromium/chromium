// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helper module which must be loaded by tests of type
 * "mojo_webui" to have proper support in test_api.js.
 */

import {TestRunner} from './web_ui_test.mojom-webui.js';

/**
 * Reports a test result using the TestRunner Mojo interface.
 * @param {string=} opt_result
 */
window.reportMojoWebUITestResult = function(opt_result) {
  const runner = TestRunner.getRemote();
  runner.testComplete(opt_result);
};

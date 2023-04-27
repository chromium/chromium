// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {ParentAccessController} from 'chrome://parent-access/parent_access_controller.js';
import {assert} from 'chrome://resources/ash/common/assert.js';


const TARGET_URL = 'chrome://test/chromeos/parent_access/test_content.html';

window.parent_access_controller_tests = {};
parent_access_controller_tests.suiteName = 'ParentAccessControllerTest';

/** @enum {string} */
parent_access_controller_tests.TestNames = {
  ParentAccessCallbackReceivedFnCalled:
      'tests that parent access result was passed through',
};

suite(parent_access_controller_tests.suiteName, function() {
  let element;
  let parentAccessController;

  setup(function() {
    document.body.innerHTML = window.trustedTypes.emptyHTML;
    // The test uses an iframe instead of a webview because a webview
    // can't load content from a chrome:// URL. This is OK because the
    // functionality being tested here doesn't rely on webview features.
    element = document.createElement('iframe');
    element.src = TARGET_URL;
    document.body.appendChild(element);
  });

  test(
      parent_access_controller_tests.TestNames
          .ParentAccessCallbackReceivedFnCalled,
      async function() {
        parentAccessController = new ParentAccessController(
            element, 'chrome://test', 'chrome://test');

        const parentAccessResult = await Promise.race([
          parentAccessController.whenParentAccessCallbackReceived(),
          parentAccessController.whenInitializationError(),
        ]);

        // Verify that the result received is the one set in test_content.html
        assertEquals(0, parentAccessResult.message.type);
      });
});

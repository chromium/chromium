// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://parent-access/parent_access_disabled.js';
import 'chrome://parent-access/strings.m.js';

import {ParentAccessResult} from 'chrome://parent-access/parent_access_ui.mojom-webui.js';
import {setParentAccessUIHandlerForTest} from 'chrome://parent-access/parent_access_ui_handler.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {buildExtensionApprovalsParamsWithoutPermissions} from './parent_access_test_utils.js';
import {TestParentAccessUIHandler} from './test_parent_access_ui_handler.js';

window.parent_access_disabled_tests = {};
parent_access_disabled_tests.suiteName = 'ParentAccessDisabledTest';

/** @enum {string} */
parent_access_disabled_tests.TestNames = {
  TestOkButton: 'Test the approve button in the after flow',
};

suite(parent_access_disabled_tests.suiteName, function() {
  setup(function() {
    PolymerTest.clearBody();
  });

  test(parent_access_disabled_tests.TestNames.TestOkButton, async () => {
    // Set up the ParentAccessParams for the extension approvals flow.
    const handler = new TestParentAccessUIHandler();
    handler.setParentAccessParams(
        buildExtensionApprovalsParamsWithoutPermissions(
            /**isDisabled=*/ true));
    setParentAccessUIHandlerForTest(handler);

    // Render ParentAccessDisabled element
    const parentAccessDisabled =
        document.createElement('parent-access-disabled');
    document.body.appendChild(parentAccessDisabled);
    await flushTasks();

    // Assert the disabled result is sent when the OK button is clicked.
    assertEquals(handler.getCallCount('onParentAccessDone'), 0);
    const okButton =
        parentAccessDisabled.shadowRoot.querySelector('.action-button');
    okButton.click();
    assertEquals(handler.getCallCount('onParentAccessDone'), 1);
    assertEquals(
        handler.getArgs('onParentAccessDone')[0], ParentAccessResult.kDisabled);
  });
});

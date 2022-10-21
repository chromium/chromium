// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../mojo_webui_test_support.js';
import 'chrome://parent-access/parent_access_after.js';
import 'chrome://parent-access/strings.m.js';

import {ParentAccessResult} from 'chrome://parent-access/parent_access_ui.mojom-webui.js';
import {setParentAccessUIHandlerForTest} from 'chrome://parent-access/parent_access_ui_handler.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {buildWebApprovalsParams} from './parent_access_test_utils.js';
import {TestParentAccessUIHandler} from './test_parent_access_ui_handler.js';

window.parent_access_after_tests = {};
parent_access_after_tests.suiteName = 'ParentAccessAfterTest';

/** @enum {string} */
parent_access_after_tests.TestNames = {
  TestApproveButton: 'Test the approve button in the after flow',
  TestDenyButton: 'Test the deny button in the after flow',
};

suite(parent_access_after_tests.suiteName, function() {
  setup(function() {
    PolymerTest.clearBody();
  });

  test(parent_access_after_tests.TestNames.TestApproveButton, async () => {
    // Set up the ParentAccessParams and handler for the web approvals flow.
    const handler = new TestParentAccessUIHandler();
    handler.setParentAccessParams(buildWebApprovalsParams());
    setParentAccessUIHandlerForTest(handler);

    // Render ParentAccessAfter element
    const parentAccessAfter = document.createElement('parent-access-after');
    document.body.appendChild(parentAccessAfter);
    await flushTasks();

    // Assert approve flow completes when the approve button is clicked.
    assertEquals(handler.getCallCount('onParentAccessDone'), 0);
    const approveButton =
        parentAccessAfter.shadowRoot.querySelector('.action-button');
    approveButton.click();
    assertEquals(handler.getCallCount('onParentAccessDone'), 1);
    assertEquals(
        handler.getArgs('onParentAccessDone')[0], ParentAccessResult.kApproved);
  });

  test(parent_access_after_tests.TestNames.TestDenyButton, async () => {
    // Set up the ParentAccessParams and handler for the web approvals flow.
    const handler = new TestParentAccessUIHandler();
    handler.setParentAccessParams(buildWebApprovalsParams());
    setParentAccessUIHandlerForTest(handler);

    // Render ParentAccessAfter element
    const parentAccessAfter = document.createElement('parent-access-after');
    document.body.appendChild(parentAccessAfter);
    await flushTasks();

    // Assert deny flow completes when the deny button is clicked.
    assertEquals(handler.getCallCount('onParentAccessDone'), 0);
    const denyButton =
        parentAccessAfter.shadowRoot.querySelector('.decline-button');
    denyButton.click();
    assertEquals(handler.getCallCount('onParentAccessDone'), 1);
    assertEquals(
        handler.getArgs('onParentAccessDone')[0], ParentAccessResult.kDeclined);
  });
});

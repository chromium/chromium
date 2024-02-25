// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';
import 'chrome://parent-access/parent_access_after.js';
import 'chrome://parent-access/strings.m.js';

import {ParentAccessResult} from 'chrome://parent-access/parent_access_ui.mojom-webui.js';
import {resetParentAccessHandlerForTest, setParentAccessUiHandlerForTest} from 'chrome://parent-access/parent_access_ui_handler.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {buildWebApprovalsParams, clearDocumentBody} from './parent_access_test_utils.js';
import {TestParentAccessUiHandler} from './test_parent_access_ui_handler.js';

suite('ParentAccessAfterTest', function() {
  setup(() => {
    clearDocumentBody();
  });

  teardown(() => {
    resetParentAccessHandlerForTest();
  });

  test('TestApproveButton', async () => {
    // Set up the ParentAccessParams and handler for the web approvals flow.
    const handler = new TestParentAccessUiHandler();
    handler.setParentAccessParams(buildWebApprovalsParams());
    setParentAccessUiHandlerForTest(handler);

    // Render ParentAccessAfter element
    const parentAccessAfter = document.createElement('parent-access-after');
    document.body.appendChild(parentAccessAfter);
    await flushTasks();

    // Assert approve flow completes when the approve button is clicked.
    assertEquals(handler.getCallCount('onParentAccessDone'), 0);
    const approveButton =
        parentAccessAfter.shadowRoot!.querySelector<HTMLElement>(
            '.action-button')!;
    approveButton.click();
    assertEquals(handler.getCallCount('onParentAccessDone'), 1);
    assertEquals(
        handler.getArgs('onParentAccessDone')[0], ParentAccessResult.kApproved);
  });

  test('TestDenyButton', async () => {
    // Set up the ParentAccessParams and handler for the web approvals flow.
    const handler = new TestParentAccessUiHandler();
    handler.setParentAccessParams(buildWebApprovalsParams());
    setParentAccessUiHandlerForTest(handler);

    // Render ParentAccessAfter element
    const parentAccessAfter = document.createElement('parent-access-after');
    document.body.appendChild(parentAccessAfter);
    await flushTasks();

    // Assert deny flow completes when the deny button is clicked.
    assertEquals(handler.getCallCount('onParentAccessDone'), 0);
    const denyButton = parentAccessAfter.shadowRoot!.querySelector<HTMLElement>(
        '.decline-button')!;
    denyButton.click();
    assertEquals(handler.getCallCount('onParentAccessDone'), 1);
    assertEquals(
        handler.getArgs('onParentAccessDone')[0], ParentAccessResult.kDeclined);
  });
});

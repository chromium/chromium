// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';
import 'chrome://parent-access/parent_access_before.js';
import 'chrome://parent-access/strings.m.js';

import {resetParentAccessHandlerForTest, setParentAccessUiHandlerForTest} from 'chrome://parent-access/parent_access_ui_handler.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {buildExtensionApprovalsParamsWithoutPermissions, buildExtensionApprovalsParamsWithPermissions, clearDocumentBody} from './parent_access_test_utils.js';
import {TestParentAccessUiHandler} from './test_parent_access_ui_handler.js';

suite('ExtensionApprovalsTest', function() {
  setup(function() {
    clearDocumentBody();
  });

  teardown(() => {
    resetParentAccessHandlerForTest();
  });

  test('TestNoPermissions', async () => {
    // Set up the ParentAccessParams and handler for the web approvals flow.
    const handler = new TestParentAccessUiHandler();
    handler.setParentAccessParams(
        buildExtensionApprovalsParamsWithoutPermissions());
    setParentAccessUiHandlerForTest(handler);

    // Render ParentAccessAfter element
    const parentAccessBefore = document.createElement('parent-access-before');
    document.body.appendChild(parentAccessBefore);
    await flushTasks();

    // Verify that no permissions have rendered.
    const extensionBefore = parentAccessBefore.shadowRoot!.querySelector(
        'extension-approvals-before')!;
    const template = extensionBefore.shadowRoot!.querySelector(
        'extension-approvals-template')!;
    assertNotEquals(
        null, template.shadowRoot!.querySelector('#extension-title'));
    assertEquals(
        null,
        template.shadowRoot!.querySelector('#extension-permissions-container'));
  });

  test('TestWithPermissions', async () => {
    // Set up the ParentAccessParams and handler for the web approvals flow.
    const handler = new TestParentAccessUiHandler();
    handler.setParentAccessParams(
        buildExtensionApprovalsParamsWithPermissions());
    setParentAccessUiHandlerForTest(handler);

    // Render ParentAccessAfter element
    const parentAccessBefore = document.createElement('parent-access-before');
    document.body.appendChild(parentAccessBefore);
    await flushTasks();

    // Verify that a permission has been rendered.
    const extensionBefore = parentAccessBefore.shadowRoot!.querySelector(
        'extension-approvals-before')!;
    const template = extensionBefore.shadowRoot!.querySelector(
        'extension-approvals-template')!;
    assertNotEquals(
        null, template.shadowRoot!.querySelector('#extension-title'));
    assertNotEquals(
        null,
        template.shadowRoot!.querySelector('#extension-permissions-container'));
    assertNotEquals(
        null, template.shadowRoot!.querySelector('extension-permission'));
    assertEquals(null, template.shadowRoot!.querySelector('#details'));
  });

  test('TestWithPermissionsAndDetails', async () => {
    // Set up the ParentAccessParams and handler for the web approvals flow.
    const handler = new TestParentAccessUiHandler();
    handler.setParentAccessParams(buildExtensionApprovalsParamsWithPermissions(
        /*isDisabled=*/ false, /*hasDetails=*/ true));
    setParentAccessUiHandlerForTest(handler);

    // Render ParentAccessAfter element
    const parentAccessBefore = document.createElement('parent-access-before');
    document.body.appendChild(parentAccessBefore);
    await flushTasks();

    // Verify that a permission has been rendered.
    const extensionBefore = parentAccessBefore.shadowRoot!.querySelector(
        'extension-approvals-before')!;
    const template = extensionBefore.shadowRoot!.querySelector(
        'extension-approvals-template')!;
    assertNotEquals(
        null, template.shadowRoot!.querySelector('#extension-title'));
    assertNotEquals(
        null,
        template.shadowRoot!.querySelector('#extension-permissions-container'));

    // Verify the show and hide details buttons are working.
    const permission =
        template.shadowRoot!.querySelector('extension-permission')!;
    const showDetails =
        permission.shadowRoot!.querySelector<HTMLElement>('#show-details')!;
    const hideDetails =
        permission.shadowRoot!.querySelector<HTMLElement>('#hide-details')!;
    assertFalse(showDetails.hidden);
    assertTrue(hideDetails.hidden);

    showDetails.click();
    assertTrue(showDetails.hidden);
    assertFalse(hideDetails.hidden);
  });
});

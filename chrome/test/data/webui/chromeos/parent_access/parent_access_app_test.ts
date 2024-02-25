// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';
import 'chrome://parent-access/parent_access_app.js';
import 'chrome://parent-access/strings.m.js';

import {ParentAccessApp, ParentAccessEvent, Screens} from 'chrome://parent-access/parent_access_app.js';
import {GetOauthTokenStatus} from 'chrome://parent-access/parent_access_ui.mojom-webui.js';
import {resetParentAccessHandlerForTest, setParentAccessUiHandlerForTest} from 'chrome://parent-access/parent_access_ui_handler.js';
import {assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {buildExtensionApprovalsParamsWithPermissions, buildWebApprovalsParams, clearDocumentBody} from './parent_access_test_utils.js';
import {TestParentAccessUiHandler} from './test_parent_access_ui_handler.js';

suite('ParentAccessAppTest', function() {
  setup(function() {
    clearDocumentBody();
  });

  teardown(() => {
    resetParentAccessHandlerForTest();
  });

  test('TestShowWebApprovalsAfterFlow', async () => {
    // Set up the TestParentAccessUiHandler
    const handler = new TestParentAccessUiHandler();
    handler.setParentAccessParams(buildWebApprovalsParams());
    handler.setOauthTokenStatus('token', GetOauthTokenStatus.kSuccess);
    setParentAccessUiHandlerForTest(handler);

    // Create app element.
    const parentAccessApp = new ParentAccessApp();
    document.body.appendChild(parentAccessApp);
    await flushTasks();

    // Verify online flow is showing and switch to the after screen.
    assertEquals(
        parentAccessApp.getCurrentScreenForTest(), Screens.AUTHENTICATION_FLOW);
    parentAccessApp.dispatchEvent(
        new CustomEvent(ParentAccessEvent.SHOW_AFTER));
    await flushTasks();

    // Verify after flow is showing.
    assertEquals(parentAccessApp.getCurrentScreenForTest(), Screens.AFTER_FLOW);
    // Verify the local web approvals after screen is showing.
    const parentAccessAfter =
        parentAccessApp.shadowRoot!.querySelector('parent-access-after')!;
    const webApprovalsAfter = parentAccessAfter.shadowRoot!.querySelector(
        'local-web-approvals-after');
    assertNotEquals(null, webApprovalsAfter);
  });

  test('TestShowExtensionApprovalsFlow', async () => {
    // Set up the TestParentAccessUiHandler
    const handler = new TestParentAccessUiHandler();
    handler.setParentAccessParams(
        buildExtensionApprovalsParamsWithPermissions());
    handler.setOauthTokenStatus('token', GetOauthTokenStatus.kSuccess);
    setParentAccessUiHandlerForTest(handler);

    // Create app element.
    const parentAccessApp = new ParentAccessApp();
    document.body.appendChild(parentAccessApp);
    await flushTasks();

    // Verify before flow is showing and switch to the authentication
    // screen.
    assertEquals(
        parentAccessApp.getCurrentScreenForTest(), Screens.BEFORE_FLOW);
    // Verify the extension approvals before screen is showing.
    const parentAccessBefore =
        parentAccessApp.shadowRoot!.querySelector('parent-access-before')!;
    const extensionApprovalsBefore =
        parentAccessBefore.shadowRoot!.querySelector(
            'extension-approvals-before');
    assertNotEquals(null, extensionApprovalsBefore);

    const askParentButton =
        parentAccessBefore.shadowRoot!.querySelector<HTMLElement>(
            '.action-button')!;
    askParentButton.click();
    assertEquals(handler.getCallCount('onBeforeScreenDone'), 1);
    await flushTasks();

    // Verify online flow is showing and switch to the after screen.
    assertEquals(
        parentAccessApp.getCurrentScreenForTest(), Screens.AUTHENTICATION_FLOW);
    parentAccessApp.dispatchEvent(
        new CustomEvent(ParentAccessEvent.SHOW_AFTER));
    await flushTasks();

    // Verify after flow is showing.
    assertEquals(parentAccessApp.getCurrentScreenForTest(), Screens.AFTER_FLOW);
    // Verify the extension approvals after screen is showing.
    const parentAccessAfter =
        parentAccessApp.shadowRoot!.querySelector('parent-access-after')!;
    const extensionApprovalsAfter = parentAccessAfter.shadowRoot!.querySelector(
        'extension-approvals-after');
    assertNotEquals(null, extensionApprovalsAfter);
  });

  test('TestShowExtensionApprovalsDisabledScreen', async () => {
    // Set up the TestParentAccessUiHandler
    const handler = new TestParentAccessUiHandler();
    handler.setParentAccessParams(buildExtensionApprovalsParamsWithPermissions(
        /*is_disabled=*/ true));
    handler.setOauthTokenStatus('token', GetOauthTokenStatus.kSuccess);
    setParentAccessUiHandlerForTest(handler);

    // Create app element.
    const parentAccessApp = new ParentAccessApp();
    document.body.appendChild(parentAccessApp);
    await flushTasks();

    // Verify disabled flow is showing.
    assertEquals(parentAccessApp.getCurrentScreenForTest(), Screens.DISABLED);
    // Verify extension approvals disabled screen is showing.
    const parentAccessAfter =
        parentAccessApp.shadowRoot!.querySelector('parent-access-disabled')!;
    const extensionApprovalsDisabled =
        parentAccessAfter.shadowRoot!.querySelector(
            'extension-approvals-disabled');
    assertNotEquals(null, extensionApprovalsDisabled);

    // Verify disabled screen still showing after triggering offline event.
    window.dispatchEvent(new Event('offline'));
    await flushTasks();
    assertEquals(parentAccessApp.getCurrentScreenForTest(), Screens.DISABLED);
  });

  test('TestShowErrorScreenOnOauthFailure', async () => {
    // Set up the TestParentAccessUiHandler
    const handler = new TestParentAccessUiHandler();
    handler.setParentAccessParams(buildWebApprovalsParams());
    handler.setOauthTokenStatus('token', GetOauthTokenStatus.kError);
    setParentAccessUiHandlerForTest(handler);

    // Create app element.
    const parentAccessApp = new ParentAccessApp();
    document.body.appendChild(parentAccessApp);
    await flushTasks();

    // Verify error screen is showing.
    assertEquals(parentAccessApp.getCurrentScreenForTest(), Screens.ERROR);
  });

  test('TestWebApprovalsOffline', async () => {
    // Set up the ParentAccessParams for the web approvals flow.
    const handler = new TestParentAccessUiHandler();
    handler.setParentAccessParams(buildWebApprovalsParams());
    handler.setOauthTokenStatus('token', GetOauthTokenStatus.kSuccess);
    setParentAccessUiHandlerForTest(handler);

    // Create app element.
    const parentAccessApp = new ParentAccessApp();
    document.body.appendChild(parentAccessApp);
    await flushTasks();

    // Verify online flow is showing
    assertEquals(
        parentAccessApp.getCurrentScreenForTest(), Screens.AUTHENTICATION_FLOW);

    // Verify offline screen shows when window triggers offline event
    window.dispatchEvent(new Event('offline'));
    await flushTasks();
    assertEquals(parentAccessApp.getCurrentScreenForTest(), Screens.OFFLINE);

    // Verify online screen shows when window triggers online event after being
    // offline
    window.dispatchEvent(new Event('online'));
    await flushTasks();
    assertEquals(
        parentAccessApp.getCurrentScreenForTest(), Screens.AUTHENTICATION_FLOW);
  });

  test('TestErrorStateIsTerminal', async () => {
    // Set up the TestParentAccessUiHandler
    const handler = new TestParentAccessUiHandler();
    handler.setParentAccessParams(buildWebApprovalsParams());
    handler.setOauthTokenStatus('token', GetOauthTokenStatus.kError);
    setParentAccessUiHandlerForTest(handler);

    // Create app element.
    const parentAccessApp = new ParentAccessApp();
    document.body.appendChild(parentAccessApp);
    await flushTasks();

    // Verify error screen is showing.
    assertEquals(parentAccessApp.getCurrentScreenForTest(), Screens.ERROR);

    // Verify error screen still showing after triggering offline event.
    window.dispatchEvent(new Event('offline'));
    await flushTasks();
    assertEquals(parentAccessApp.getCurrentScreenForTest(), Screens.ERROR);

    // Verify error screen still showing after triggering online event.
    window.dispatchEvent(new Event('online'));
    await flushTasks();
    assertEquals(parentAccessApp.getCurrentScreenForTest(), Screens.ERROR);
  });
});

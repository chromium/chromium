// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://parent-access/parent_access_app.js';
import 'chrome://parent-access/strings.m.js';

import {Screens} from 'chrome://parent-access/parent_access_app.js';
import {GetOAuthTokenStatus} from 'chrome://parent-access/parent_access_ui.mojom-webui.js';
import {setParentAccessUIHandlerForTest} from 'chrome://parent-access/parent_access_ui_handler.js';
import {assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {buildExtensionApprovalsParams, buildWebApprovalsParams} from './parent_access_test_utils.js';
import {TestParentAccessUIHandler} from './test_parent_access_ui_handler.js';

window.parent_access_app_tests = {};
parent_access_app_tests.suiteName = 'ParentAccessAppTest';

/** @enum {string} */
parent_access_app_tests.TestNames = {
  TestShowWebApprovalsAfterFlow:
      'Tests that the web approvals after flow is shown',
  TestShowExtensionApprovalsFlow:
      'Tests that the extension approvals flow is shown',
  TestShowExtensionApprovalsDisabledScreen:
      'Tests that the extensions disabled flow is shown and is terminal',
  TestShowErrorScreenOnOAuthFailure: 'Tests that the error screen is shown',
  TestWebApprovalsOffline:
      'Tests that dialog switches in/out of offline screen',
  TestErrorStateIsTerminal:
      'Tests that going offline/online does not switch away from error screen',
};

suite(parent_access_app_tests.suiteName, function() {
  setup(function() {
    PolymerTest.clearBody();
  });

  test(
      parent_access_app_tests.TestNames.TestShowWebApprovalsAfterFlow,
      async () => {
        // Set up the TestParentAccessUIHandler
        const handler = new TestParentAccessUIHandler();
        handler.setParentAccessParams(buildWebApprovalsParams());
        handler.setOAuthTokenStatus('token', GetOAuthTokenStatus.kSuccess);
        setParentAccessUIHandlerForTest(handler);

        // Create app element.
        const parentAccessApp = document.createElement('parent-access-app');
        document.body.appendChild(parentAccessApp);
        await flushTasks();

        // Verify online flow is showing and switch to the after screen.
        assertEquals(
            parentAccessApp.currentScreen_, Screens.AUTHENTICATION_FLOW);
        parentAccessApp.dispatchEvent(new CustomEvent('show-after'));
        await flushTasks();

        // Verify after flow is showing.
        assertEquals(parentAccessApp.currentScreen_, Screens.AFTER_FLOW);
        // Verify the local web approvals after screen is showing.
        const parentAccessAfter =
            parentAccessApp.shadowRoot.querySelector('parent-access-after');
        const webApprovalsAfter = parentAccessAfter.shadowRoot.querySelector(
            'local-web-approvals-after');
        assertNotEquals(null, webApprovalsAfter);
      });

  test(
      parent_access_app_tests.TestNames.TestShowExtensionApprovalsFlow,
      async () => {
        // Set up the TestParentAccessUIHandler
        const handler = new TestParentAccessUIHandler();
        handler.setParentAccessParams(
            buildExtensionApprovalsParams(/*is_disabled=*/ false));
        handler.setOAuthTokenStatus('token', GetOAuthTokenStatus.kSuccess);
        setParentAccessUIHandlerForTest(handler);

        // Create app element.
        const parentAccessApp = document.createElement('parent-access-app');
        document.body.appendChild(parentAccessApp);
        await flushTasks();

        // Verify online flow is showing and switch to the after screen.
        assertEquals(parentAccessApp.currentScreen_, Screens.BEFORE_FLOW);
        parentAccessApp.dispatchEvent(
            new CustomEvent('show-authentication-flow'));
        await flushTasks();

        // Verify online flow is showing and switch to the after screen.
        assertEquals(
            parentAccessApp.currentScreen_, Screens.AUTHENTICATION_FLOW);
      });

  test(
      parent_access_app_tests.TestNames
          .TestShowExtensionApprovalsDisabledScreen,
      async () => {
        // Set up the TestParentAccessUIHandler
        const handler = new TestParentAccessUIHandler();
        handler.setParentAccessParams(
            buildExtensionApprovalsParams(/*is_disabled=*/ true));
        handler.setOAuthTokenStatus('token', GetOAuthTokenStatus.kSuccess);
        setParentAccessUIHandlerForTest(handler);

        // Create app element.
        const parentAccessApp = document.createElement('parent-access-app');
        document.body.appendChild(parentAccessApp);
        await flushTasks();

        // Verify disabled flow is showing.
        assertEquals(parentAccessApp.currentScreen_, Screens.DISABLED);
        // Verify extension approvals disabled screen is showing.
        const parentAccessAfter =
            parentAccessApp.shadowRoot.querySelector('parent-access-disabled');
        const extensionApprovalsDisabled =
            parentAccessAfter.shadowRoot.querySelector(
                'extension-approvals-disabled');
        assertNotEquals(null, extensionApprovalsDisabled);

        // Verify disabled screen still showing after triggering offline event.
        window.dispatchEvent(new Event('offline'));
        await flushTasks();
        assertEquals(parentAccessApp.currentScreen_, Screens.DISABLED);
      });

  test(
      parent_access_app_tests.TestNames.TestShowErrorScreenOnOAuthFailure,
      async () => {
        // Set up the TestParentAccessUIHandler
        const handler = new TestParentAccessUIHandler();
        handler.setParentAccessParams(buildWebApprovalsParams());
        handler.setOAuthTokenStatus('token', GetOAuthTokenStatus.kError);
        setParentAccessUIHandlerForTest(handler);

        // Create app element.
        const parentAccessApp = document.createElement('parent-access-app');
        document.body.appendChild(parentAccessApp);
        await flushTasks();

        // Verify error screen is showing.
        assertEquals(parentAccessApp.currentScreen_, Screens.ERROR);
      });

  test(parent_access_app_tests.TestNames.TestWebApprovalsOffline, async () => {
    // Set up the ParentAccessParams for the web approvals flow.
    const handler = new TestParentAccessUIHandler();
    handler.setParentAccessParams(buildWebApprovalsParams());
    handler.setOAuthTokenStatus('token', GetOAuthTokenStatus.kSuccess);
    setParentAccessUIHandlerForTest(handler);

    // Create app element.
    const parentAccessApp = document.createElement('parent-access-app');
    document.body.appendChild(parentAccessApp);
    await flushTasks();

    // Verify online flow is showing
    assertEquals(parentAccessApp.currentScreen_, Screens.AUTHENTICATION_FLOW);

    // Verify offline screen shows when window triggers offline event
    window.dispatchEvent(new Event('offline'));
    await flushTasks();
    assertEquals(parentAccessApp.currentScreen_, Screens.OFFLINE);

    // Verify online screen shows when window triggers online event after being
    // offline
    window.dispatchEvent(new Event('online'));
    await flushTasks();
    assertEquals(parentAccessApp.currentScreen_, Screens.AUTHENTICATION_FLOW);
  });

  test(parent_access_app_tests.TestNames.TestErrorStateIsTerminal, async () => {
    // Set up the TestParentAccessUIHandler
    const handler = new TestParentAccessUIHandler();
    handler.setParentAccessParams(buildWebApprovalsParams());
    handler.setOAuthTokenStatus('token', GetOAuthTokenStatus.kError);
    setParentAccessUIHandlerForTest(handler);

    // Create app element.
    const parentAccessApp = document.createElement('parent-access-app');
    document.body.appendChild(parentAccessApp);
    await flushTasks();

    // Verify error screen is showing.
    assertEquals(parentAccessApp.currentScreen_, Screens.ERROR);

    // Verify error screen still showing after triggering offline event.
    window.dispatchEvent(new Event('offline'));
    await flushTasks();
    assertEquals(parentAccessApp.currentScreen_, Screens.ERROR);

    // Verify error screen still showing after triggering online event.
    window.dispatchEvent(new Event('online'));
    await flushTasks();
    assertEquals(parentAccessApp.currentScreen_, Screens.ERROR);
  });
});

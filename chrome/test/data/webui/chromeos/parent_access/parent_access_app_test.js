// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../mojo_webui_test_support.js';
import 'chrome://parent-access/parent_access_app.js';
import 'chrome://parent-access/strings.m.js';

import {Screens} from 'chrome://parent-access/parent_access_app.js';
import {setParentAccessUIHandlerForTest} from 'chrome://parent-access/parent_access_ui_handler.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertNotEquals} from '../../chai_assert.js';

import {buildWebApprovalsParams} from './parent_access_test_utils.js';
import {TestParentAccessUIHandler} from './test_parent_access_ui_handler.js';

window.parent_access_app_tests = {};
parent_access_app_tests.suiteName = 'ParentAccessAppTest';

/** @enum {string} */
parent_access_app_tests.TestNames = {
  TestShowWebApprovalsAfterFlow:
      'Tests that the web approvals after flow is shown',
};

suite(parent_access_app_tests.suiteName, function() {
  setup(function() {
    PolymerTest.clearBody();
  });

  test(
      parent_access_app_tests.TestNames.TestShowWebApprovalsAfterFlow,
      async () => {
        // Set up the ParentAccessParams for the web approvals flow.
        const handler = new TestParentAccessUIHandler();
        handler.setParentAccessParams(buildWebApprovalsParams());
        setParentAccessUIHandlerForTest(handler);

        // Create app element.
        const parentAccessApp = document.createElement('parent-access-app');
        document.body.appendChild(parentAccessApp);
        await flushTasks();

        // Verify online flow is showing and switch to the after screen.
        assertEquals(parentAccessApp.currentScreen_, Screens.ONLINE_FLOW);
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
});

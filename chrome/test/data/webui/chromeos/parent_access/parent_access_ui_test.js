// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://parent-access/parent_access_ui.js';
import 'chrome://parent-access/strings.m.js';

import {setParentAccessUIHandlerForTest} from 'chrome://parent-access/parent_access_ui_handler.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestParentAccessUIHandler} from './test_parent_access_ui_handler.js';

window.parent_access_ui_tests = {};
parent_access_ui_tests.suiteName = 'ParentAccessUITest';

/** @enum {string} */
parent_access_ui_tests.TestNames = {
  TestIsAllowedRequest: 'Tests that only allowed requests are allowed',
  TestShouldReceiveAuthHeader:
      'Tests that only the intended origins receive the auth header',
};

suite(parent_access_ui_tests.suiteName, function() {
  let parentAccessUI;
  let handler;

  setup(function() {
    PolymerTest.clearBody();
    handler = new TestParentAccessUIHandler();
    setParentAccessUIHandlerForTest(handler);
    parentAccessUI = document.createElement('parent-access-ui');
    document.body.appendChild(parentAccessUI);
    flush();
  });

  test(parent_access_ui_tests.TestNames.TestIsAllowedRequest, async function() {
    // HTTPS fetches to allowlisted domains are allowed.
    assertTrue(parentAccessUI.isAllowedRequest('https://families.google.com'));
    assertTrue(parentAccessUI.isAllowedRequest('https://somehost.gstatic.com'));
    assertTrue(parentAccessUI.isAllowedRequest(
        'https://somehost.googleusercontent.com'));
    assertTrue(
        parentAccessUI.isAllowedRequest('https://somehost.googleapis.com'));

    // HTTP not allowed for allowlisted hosts that aren't the webview URL.
    assertFalse(parentAccessUI.isAllowedRequest('http://families.google.com'));
    assertFalse(parentAccessUI.isAllowedRequest('http://somehost.gstatic.com'));
    assertFalse(
        parentAccessUI.isAllowedRequest('http://somehost.googleapis.com'));
    assertFalse(parentAccessUI.isAllowedRequest(
        'http://somehost.googleusercontent.com'));

    // Request not allowed for non-allowlisted hosts, whether https or http.
    assertFalse(parentAccessUI.isAllowedRequest('https://www.example.com'));
    assertFalse(parentAccessUI.isAllowedRequest('http://www.example.com'));

    // Exception to HTTPS for localhost for local server development.
    assertTrue(parentAccessUI.isAllowedRequest('http://localhost:9879'));
  });

  test(
      parent_access_ui_tests.TestNames.TestShouldReceiveAuthHeader,
      async function() {
        // Auth header should be sent to webview URL.
        const webviewUrl = (await handler.getParentAccessURL()).url;
        assertTrue(parentAccessUI.shouldReceiveAuthHeader(webviewUrl));

        // Nothing else should receive the auth header.
        assertFalse(
            parentAccessUI.shouldReceiveAuthHeader('https://www.google.com'));
        assertFalse(parentAccessUI.shouldReceiveAuthHeader(
            'https://somehost.gstatic.com'));
        assertFalse(parentAccessUI.shouldReceiveAuthHeader(
            'https://somehost.googleapis.com'));
        assertFalse(parentAccessUI.shouldReceiveAuthHeader(
            'https://somehost.googleusercontent.com'));
      });
});

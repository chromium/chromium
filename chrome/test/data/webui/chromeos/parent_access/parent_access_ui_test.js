// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import '../../mojo_webui_test_support.js';
import 'chrome://parent-access/parent_access_ui.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

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

  suiteSetup(function() {
    loadTimeData.overrideValues(
        {webviewUrl: 'chrome://about', eventOriginFilter: 'chrome://about'});
  });

  setup(function() {
    PolymerTest.clearBody();
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
    assertTrue(
        parentAccessUI.isAllowedRequest(loadTimeData.getString('webviewUrl')));
  });

  test(
      parent_access_ui_tests.TestNames.TestShouldReceiveAuthHeader,
      async function() {
        // Auth header should be sent to webview URL.
        assertTrue(parentAccessUI.shouldReceiveAuthHeader(
            loadTimeData.getString('webviewUrl')));

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

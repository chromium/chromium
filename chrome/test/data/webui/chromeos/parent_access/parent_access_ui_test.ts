// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';
import 'chrome://parent-access/parent_access_ui.js';
import 'chrome://parent-access/strings.m.js';

import {ParentAccessUi} from 'chrome://parent-access/parent_access_ui.js';
import {ParentAccessUiHandlerInterface} from 'chrome://parent-access/parent_access_ui.mojom-webui.js';
import {resetParentAccessHandlerForTest, setParentAccessUiHandlerForTest} from 'chrome://parent-access/parent_access_ui_handler.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {clearDocumentBody} from './parent_access_test_utils.js';
import {TestParentAccessUiHandler} from './test_parent_access_ui_handler.js';


suite('ParentAccessUiTest', function() {
  let parentAccessUi: ParentAccessUi;
  let handler: ParentAccessUiHandlerInterface;

  setup(function() {
    clearDocumentBody();
    handler = new TestParentAccessUiHandler();
    setParentAccessUiHandlerForTest(handler);
    parentAccessUi = new ParentAccessUi();
    document.body.appendChild(parentAccessUi);
    flush();
  });

  teardown(() => {
    parentAccessUi.remove();
    resetParentAccessHandlerForTest();
  });

  // Tests that only allowed requests are allowed.
  test('TestIsAllowedRequest', async () => {
    // HTTPS fetches to allowlisted domains are allowed.
    assertTrue(parentAccessUi.isAllowedRequest('https://families.google.com'));
    assertTrue(parentAccessUi.isAllowedRequest('https://somehost.gstatic.com'));
    assertTrue(parentAccessUi.isAllowedRequest(
        'https://somehost.googleusercontent.com'));
    assertTrue(
        parentAccessUi.isAllowedRequest('https://somehost.googleapis.com'));

    // HTTP not allowed for allowlisted hosts that aren't the webview URL.
    assertFalse(parentAccessUi.isAllowedRequest('http://families.google.com'));
    assertFalse(parentAccessUi.isAllowedRequest('http://somehost.gstatic.com'));
    assertFalse(
        parentAccessUi.isAllowedRequest('http://somehost.googleapis.com'));
    assertFalse(parentAccessUi.isAllowedRequest(
        'http://somehost.googleusercontent.com'));

    // Request not allowed for non-allowlisted hosts, whether https or http.
    assertFalse(parentAccessUi.isAllowedRequest('https://www.example.com'));
    assertFalse(parentAccessUi.isAllowedRequest('http://www.example.com'));

    // Exception to HTTPS for localhost for local server development.
    assertTrue(parentAccessUi.isAllowedRequest('http://localhost:9879'));
  });

  // Tests that only the intended origins receive the auth header.
  test('TestShouldReceiveAuthHeader', async function() {
    // Auth header should be sent to webview URL.
    const webviewUrl = (await handler.getParentAccessUrl()).url;
    assertTrue(parentAccessUi.shouldReceiveAuthHeader(webviewUrl));

    // Nothing else should receive the auth header.
    assertFalse(
        parentAccessUi.shouldReceiveAuthHeader('https://www.google.com'));
    assertFalse(
        parentAccessUi.shouldReceiveAuthHeader('https://somehost.gstatic.com'));
    assertFalse(parentAccessUi.shouldReceiveAuthHeader(
        'https://somehost.googleapis.com'));
    assertFalse(parentAccessUi.shouldReceiveAuthHeader(
        'https://somehost.googleusercontent.com'));
  });
});

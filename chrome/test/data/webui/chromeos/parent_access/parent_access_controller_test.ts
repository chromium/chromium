// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';
import 'chrome://parent-access/strings.m.js';

import {ParentAccessController} from 'chrome://parent-access/parent_access_controller.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {PROTO_STRING_FOR_TEST} from './parent_access_test_client.js';
import {clearDocumentBody} from './parent_access_test_utils.js';

const TARGET_URL =
    'chrome://webui-test/chromeos/parent_access/test_content.html';

suite('ParentAccessControllerTest', function() {
  let element: HTMLIFrameElement;
  let parentAccessController: ParentAccessController;

  setup(function() {
    clearDocumentBody();
    // The test uses an iframe instead of a webview because a webview
    // can't load content from a chrome:// URL. This is OK because the
    // functionality being tested here doesn't rely on webview features.
    element = document.createElement('iframe');
    element.src = TARGET_URL;
    document.body.appendChild(element);
  });

  test('ParentAccessCallbackReceivedFnCalled', async function() {
    parentAccessController = new ParentAccessController(
        element, 'chrome://webui-test', 'chrome://webui-test');

    const message = await Promise.race([
      parentAccessController.whenParentAccessCallbackReceived(),
      parentAccessController.whenInitializationError(),
    ]);

    assertEquals(PROTO_STRING_FOR_TEST, message);
  });
});

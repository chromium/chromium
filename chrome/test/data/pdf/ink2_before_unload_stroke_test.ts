// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PluginController} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {finishInkStroke, getNewTestBeforeUnloadProxy} from './test_util.js';

chrome.test.runTests([
  // Test that the save unedited dialog appears when the user navigates away
  // from the PDF and there is a stroke on a page.
  async function testBeforeUnloadStroke() {
    const toolbar = document.body.querySelector('pdf-viewer')!.$.toolbar;

    chrome.test.assertFalse(toolbar.annotationMode);

    toolbar.toggleAnnotation();
    await microtasksFinished();
    chrome.test.assertTrue(toolbar.annotationMode);

    finishInkStroke(PluginController.getInstance());

    const testProxy = getNewTestBeforeUnloadProxy();
    window.location.href = 'about:blank';

    chrome.test.assertEq(1, testProxy.getCallCount('preventDefault'));
    chrome.test.succeed();
  },
]);

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PluginController} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {finishInkStroke, getNewTestBeforeUnloadProxy} from './test_util.js';

chrome.test.runTests([
  // Test that the save unedited dialog does not appear when the user navigates
  // away from the PDF after undoing all strokes.
  async function testBeforeUnloadUndo() {
    const toolbar = document.body.querySelector('pdf-viewer')!.$.toolbar;

    chrome.test.assertFalse(toolbar.annotationMode);

    toolbar.toggleAnnotation();
    await microtasksFinished();
    chrome.test.assertTrue(toolbar.annotationMode);

    finishInkStroke(PluginController.getInstance());
    await microtasksFinished();

    toolbar.undo();

    const testProxy = getNewTestBeforeUnloadProxy();
    window.location.href = 'about:blank';

    chrome.test.assertEq(0, testProxy.getCallCount('preventDefault'));
    chrome.test.succeed();
  },
]);

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationMode, PluginController} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {getNewTestBeforeUnloadProxy, setupTestMockPluginForInk, startFinishModifiedInkStroke} from './test_util.js';

chrome.test.runTests([
  // Test that the save unedited dialog does not appear when the user navigates
  // away from the PDF after undoing all strokes.
  async function testBeforeUnloadUndo() {
    setupTestMockPluginForInk();

    const toolbar = document.body.querySelector('pdf-viewer')!.$.toolbar;

    chrome.test.assertEq(AnnotationMode.OFF, toolbar.annotationMode);

    toolbar.setAnnotationMode(AnnotationMode.DRAW);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.DRAW, toolbar.annotationMode);

    startFinishModifiedInkStroke(PluginController.getInstance());
    await microtasksFinished();

    toolbar.undo();

    const testProxy = getNewTestBeforeUnloadProxy();
    window.location.href = 'about:blank';

    chrome.test.assertEq(0, testProxy.getCallCount('preventDefault'));
    chrome.test.succeed();
  },
]);

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationMode, PluginController} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {getNewTestBeforeUnloadProxy} from './test_before_unload_proxy.js';
import {setUpTestPdfViewerPrivateProxy} from './test_pdf_viewer_private_proxy.js';
import {getRequiredElement, setupTestMockPluginForInk, startFinishModifiedInkStroke} from './test_util.js';

chrome.test.runTests([
  // Test that the unedited dialog is shown if more edits are made while the
  // upload is in progress.
  async function testSaveToDriveUploadingMoreEditsBeforeUnloadStroke() {
    setupTestMockPluginForInk();

    const viewer = document.body.querySelector('pdf-viewer')!;
    const privateProxy = setUpTestPdfViewerPrivateProxy(viewer);
    const toolbar = viewer.$.toolbar;
    const controls =
        getRequiredElement(viewer.$.toolbar, 'viewer-save-to-drive-controls');
    const actionMenu = controls.$.menu;

    chrome.test.assertEq(AnnotationMode.OFF, toolbar.annotationMode);

    toolbar.setAnnotationMode(AnnotationMode.DRAW);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.DRAW, toolbar.annotationMode);

    startFinishModifiedInkStroke(PluginController.getInstance());
    await microtasksFinished();

    // Click Save to Drive and the menu should open.
    controls.$.save.click();
    await eventToPromise('save-menu-shown-for-testing', controls);
    chrome.test.assertTrue(actionMenu.open);

    // Click on "Edited".
    const buttons = actionMenu.querySelectorAll('button');
    chrome.test.assertEq(2, buttons.length);
    buttons[0]!.click();
    await privateProxy.whenCalled('saveToDrive');
    chrome.test.assertFalse(actionMenu.open);

    // Mock that the upload is in progress.
    privateProxy.sendUploadInProgress(25, 100);
    await microtasksFinished();

    // Make more edits.
    startFinishModifiedInkStroke(PluginController.getInstance());
    await microtasksFinished();

    // Mock that the upload is completed.
    privateProxy.sendUploadCompleted();
    await microtasksFinished();

    const testProxy = getNewTestBeforeUnloadProxy();
    window.location.href = 'about:blank';

    chrome.test.assertEq(1, testProxy.getCallCount('preventDefault'));
    chrome.test.succeed();
  },
]);

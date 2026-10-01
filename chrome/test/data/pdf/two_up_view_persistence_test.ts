// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LOCAL_STORAGE_TWO_UP_VIEW_ENABLED_KEY, LocalStorageProxyImpl, PluginController, PluginControllerEventType} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestLocalStorageProxy} from './test_local_storage_proxy.js';
import {createMockPdfPluginForTest, MockDocumentDimensions} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer');
assert(viewer);

const tests = [
  /**
   * Test that toggling two up view from the toolbar tells the plugin to change
   * the layout and stores the new state in local storage, so that it can be
   * restored for the next document.
   */
  async function testTwoUpViewStateIsStored() {
    const controller = PluginController.getInstance();
    const mockPlugin = createMockPdfPluginForTest();
    controller.setPluginForTesting(mockPlugin);
    const testProxy = new TestLocalStorageProxy();
    LocalStorageProxyImpl.setInstance(testProxy);

    const button = viewer.$.toolbar.$.twoPageViewButton;

    button.click();
    chrome.test.assertEq(
        [LOCAL_STORAGE_TWO_UP_VIEW_ENABLED_KEY, '1'],
        await testProxy.whenCalled('setItem'));
    chrome.test.assertEq(
        {type: 'setTwoUpView', enableTwoUpView: true},
        mockPlugin.findMessage('setTwoUpView'));

    // Simulate the plugin applying the two up layout.
    const documentDimensions = new MockDocumentDimensions(
        200, 100,
        {direction: 0, defaultPageOrientation: 0, twoUpViewEnabled: true});
    documentDimensions.addPageForTwoUpView(0, 0, 100, 100);
    documentDimensions.addPageForTwoUpView(100, 0, 100, 100);
    controller.getEventTarget().dispatchEvent(
        new CustomEvent(PluginControllerEventType.PLUGIN_MESSAGE, {
          detail: {type: 'documentDimensions', ...documentDimensions},
        }));
    await microtasksFinished();

    mockPlugin.clearMessages();
    testProxy.resetResolver('setItem');
    button.click();
    chrome.test.assertEq(
        [LOCAL_STORAGE_TWO_UP_VIEW_ENABLED_KEY, '0'],
        await testProxy.whenCalled('setItem'));
    chrome.test.assertEq(
        {type: 'setTwoUpView', enableTwoUpView: false},
        mockPlugin.findMessage('setTwoUpView'));

    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {getNewTestBeforeUnloadProxy} from './test_before_unload_proxy.js';
import {setUpTestPdfViewerPrivateProxy} from './test_pdf_viewer_private_proxy.js';

chrome.test.runTests([
  // Test that the save unedited dialog appears when the user navigates away
  // from the PDF when Save to Drive is uploading.
  async function testSaveToDriveBeforePageLoaded() {
    const viewer = document.body.querySelector('pdf-viewer')!;
    const privateProxy = setUpTestPdfViewerPrivateProxy(viewer);

    privateProxy.sendUploadInProgress(25, 100);
    await microtasksFinished();

    const testProxy = getNewTestBeforeUnloadProxy();
    window.location.href = 'about:blank';

    chrome.test.assertEq(1, testProxy.getCallCount('preventDefault'));
    chrome.test.succeed();
  },
]);

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

const protocolVersion = '1.3';

chrome.test.getConfig(config => chrome.test.runTests([
  async function testDebuggerAttachToBlobUrls() {
    const pagePath =
        'extensions/api_test/debugger_attach_to_blob_urls/page.html';
    const topURL = `http://a.com:${config.testServer.port}/${pagePath}`;
    const tab = await openTab(topURL);
    const debuggee = {tabId: tab.id};
    await chrome.scripting.executeScript({
      target: debuggee,
      func: async () => {
        const response = await fetch('test.pdf');
        const data = await response.blob();
        const object = document.createElement('object');
        object.type = 'application/pdf';
        object.width = '250';
        object.height = '250';
        object.data = URL.createObjectURL(data);
        document.body.append(object);
        return new Promise(resolve => {
          object.addEventListener('load', () => {
            resolve();
          }, {once: true});
        });
      }
    });
    chrome.debugger.attach(debuggee, protocolVersion, () => {
      chrome.test.assertNoLastError();
      chrome.debugger.detach(debuggee, chrome.test.succeed);
    });
  }
]));

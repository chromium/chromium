// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const protocolVersion = '1.3';

chrome.test.getConfig(config => chrome.test.runTests([
  async function reloadAndResetHistory() {
    const {openTab} = await import('/_test_resources/test_util/tabs_util.js');
    const pagePath =
        'extensions/api_test/debugger_reload_and_reset_history/page.html';
    const topURL = `http://a.com:${config.testServer.port}/${pagePath}`;
    const tab = await openTab(topURL);
    const debuggee = {tabId: tab.id};
    chrome.debugger.attach(debuggee, protocolVersion, async function() {
      chrome.debugger.sendCommand(debuggee, "Page.reload", {});
      await chrome.debugger.sendCommand(
          debuggee, "Page.resetNavigationHistory", {});
      chrome.test.assertNoLastError();
      chrome.debugger.detach(debuggee, chrome.test.succeed);
    });
  }
]));

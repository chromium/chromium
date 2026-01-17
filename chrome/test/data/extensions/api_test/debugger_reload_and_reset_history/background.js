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

    await chrome.debugger.attach(debuggee, protocolVersion);
    // If we send a Page.reload command and then immediately try to prune the
    // history, history pruning should fail since the pending entry is for an
    // existing entry, which may need the earlier history as a sensible place to
    // put itself when it commits.
    await chrome.debugger.sendCommand(debuggee, "Page.reload", {});
    await chrome.test.assertPromiseRejects(
        chrome.debugger.sendCommand(
            debuggee, "Page.resetNavigationHistory", {}),
        'Error: {"code":-32000,"message":"History cannot be pruned"}');
    await chrome.debugger.detach(debuggee);
    chrome.test.succeed();
  }
]));

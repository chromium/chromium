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
    // Pause the reload's document request below, so that the navigation is
    // guaranteed to still be pending when Page.resetNavigationHistory is
    // handled. Otherwise, the reload may get ready to commit (or even commit)
    // in the meantime, making the outcome timing-dependent.
    await chrome.debugger.sendCommand(
        debuggee, 'Fetch.enable', {patterns: [{resourceType: 'Document'}]});
    const requestPaused = new Promise(resolve => {
      chrome.debugger.onEvent.addListener(
          function listener(source, method, params) {
            if (source.tabId === tab.id && method === 'Fetch.requestPaused') {
              chrome.debugger.onEvent.removeListener(listener);
              resolve(params);
            }
          });
    });
    // If we send a Page.reload command and then try to prune the history while
    // the reload is still pending, history pruning should fail since the
    // pending entry is for an existing entry, which may need the earlier
    // history as a sensible place to put itself when it commits.
    await chrome.debugger.sendCommand(debuggee, 'Page.reload', {});
    const {request} = await requestPaused;
    chrome.test.assertEq(topURL, request.url);
    await chrome.test.assertPromiseRejects(
        chrome.debugger.sendCommand(
            debuggee, 'Page.resetNavigationHistory', {}),
        'Error: {"code":-32000,"message":"History cannot be pruned"}');
    // Detaching disables the request interception, which resumes the reload.
    await chrome.debugger.detach(debuggee);
    chrome.test.succeed();
  },
]));

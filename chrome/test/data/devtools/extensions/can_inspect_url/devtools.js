// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function output(msg) {
  top.postMessage({testOutput: msg}, '*');
}

async function test() {
  // The test driver uses the URL fragment of the initial inspected page to pass
  // the URL to test against
  const newPageURL = await new Promise((resolve, reject) => {
    chrome.devtools.inspectedWindow.eval(
        'location.hash.substr(1)', {}, (res, error) => {
          if (error && error.isError)
            reject(error);
          else
            resolve(res || error.value);
        });
  });
  const inspectedTabId = chrome.devtools.inspectedWindow.tabId;
  // CDP won't let us to navigate to file: so we have to use
  // chrome.test.openFileUrl API. However the latter doesn't allow navigation to
  // chrome: scheme, so we also have to use CDP.
  if (newPageURL.startsWith('file:')) {
    chrome.test.openFileUrl(newPageURL);
  } else {
    chrome.debugger.attach(
        {tabId: inspectedTabId}, '1.3',
        () => chrome.debugger.sendCommand(
            {tabId: inspectedTabId}, 'Page.navigate', {url: newPageURL}));
  }
  await new Promise(
      resolve => chrome.tabs.onUpdated.addListener((tabId, changedProps) => {
        if (inspectedTabId !== tabId || changedProps.url !== newPageURL)
          return;
        resolve();
      }));
  // There may be a number of inherent races between the page and the DevTools
  // during the navigation. Let's do a number of eval in a hope of catching some
  // of them.
  const evaluateCallback = (result, exception) => {
    if (!exception || !exception.isError) {
      output(`FAIL: ${result || exception.value}`);
      return;
    }
    if (exception.code !== 'E_FAILED') {
      output(`FAIL: ${exception.code}`);
      return;
    }
    output('PASS');
  };
  for (let i = 0; i < 10; i++) {
    let increasingTimeout = 10;
    for (let j = 0; j < i; j++) {
      increasingTimeout *= 2;
    }
    await new Promise(resolve => {
      setTimeout(resolve, increasingTimeout);
    });
    chrome.devtools.inspectedWindow.eval('location.href', {}, evaluateCallback);
  }
}

test();

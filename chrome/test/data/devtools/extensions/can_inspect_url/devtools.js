// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let outputCalled = false;
function output(msg) {
  if (outputCalled)
    return;
  outputCalled = true;
  // Send the message periodically to ensure the test framework receives it
  // even if it sets up its listener slightly after this is called.
  setInterval(() => {
    top.postMessage({testOutput: msg}, '*');
  }, 50);
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

  const oldURL = await new Promise((resolve, reject) => {
    chrome.devtools.inspectedWindow.eval('location.href', {}, (res, error) => {
      resolve(res);
    });
  });

  const inspectedTabId = chrome.devtools.inspectedWindow.tabId;

  chrome.debugger.onDetach.addListener((source, reason) => {
    if (source.tabId === inspectedTabId) {
      output('PASS');
    }
  });

  // CDP won't let us to navigate to file: so we have to use
  // chrome.test.openFileUrl API. However the latter doesn't allow navigation to
  // chrome: scheme, so we also have to use CDP.
  if (newPageURL.startsWith('file:')) {
    chrome.test.openFileUrl(newPageURL);
  } else {
    try {
      await new Promise((resolve, reject) => {
        chrome.debugger.attach({tabId: inspectedTabId}, '1.3', () => {
          if (chrome.runtime.lastError) {
            reject(chrome.runtime.lastError);
            return;
          }
          chrome.debugger.sendCommand(
              {tabId: inspectedTabId}, 'Page.navigate', {url: newPageURL},
              () => {
                if (chrome.runtime.lastError) {
                  reject(chrome.runtime.lastError);
                  return;
                }
                resolve();
              });
        });
      });
    } catch (error) {
      output('PASS');
      return;
    }
  }

  // Poll location.href to check if we can inspect the new page.
  // If we lose access, eval fails with E_FAILED.
  for (let i = 0; i < 15; i++) {
    const increasingTimeout = 10 << Math.min(i, 10);
    await new Promise(resolve => setTimeout(resolve, increasingTimeout));

    const done = await new Promise(resolve => {
      chrome.devtools.inspectedWindow.eval(
          'location.href', {}, (result, exception) => {
            if (exception && exception.isError) {
              if (exception.code === 'E_FAILED') {
                output('PASS');
              } else {
                output(`FAIL: ${exception.code}`);
              }
              resolve(true);
              return;
            }

            if (result !== oldURL) {
              output(`FAIL: successfully inspected restricted page ${result}`);
              resolve(true);
              return;
            }

            resolve(false);
          });
    });

    if (done)
      return;
  }

  output('FAIL: timeout waiting for navigation or detach');
}

test();

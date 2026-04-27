// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  async function testFetchFulfillRequestCannotSetRestrictedCookie() {
    const config = await chrome.test.getConfig();
    const [allowedUrl, restrictedUrl] = config.customArg.split(';');

    console.log(allowedUrl, restrictedUrl)

    const tab = await chrome.tabs.create({url: allowedUrl});
    await new Promise(resolve => {
      chrome.tabs.onUpdated.addListener(function listener(tabId, info) {
        if (tabId === tab.id && info.status === 'complete') {
          chrome.tabs.onUpdated.removeListener(listener);
          resolve();
        }
      });
    });
    const target = {tabId: tab.id};

    await chrome.debugger.attach(target, '1.3');
    console.log('debugger attached');

    // Wait for the request to be intercepted.
    let interceptionPromise = new Promise(resolve => {
      let seenRestricted = false;
      let seenAllowed = false;
      chrome.debugger.onEvent.addListener(
          async function listener(src, method, params) {
            if (src.tabId !== tab.id || method !== 'Fetch.requestPaused') {
              return;
            }

            const isAllowed = params.request.url === allowedUrl;
            const cookieLine = isAllowed ?
                'allowed=test; HttpOnly; Secure; SameSite=None; Path=/; Max-Age=86400' :
                'restricted=test; HttpOnly; Secure; SameSite=None; Path=/; Max-Age=86400';

            try {
              await chrome.debugger.sendCommand(
                  target, 'Fetch.fulfillRequest', {
                    requestId: params.requestId,
                    responseCode: 200,
                    responseHeaders: [
                      {name: 'Set-Cookie', value: cookieLine},
                      {name: 'Content-Type', value: 'text/plain'},
                    ],
                    body: btoa('ok'),
                  });
              if (isAllowed) {
                seenAllowed = true;
              } else {
                seenRestricted = true;
              }
              if (seenAllowed && seenRestricted) {
                chrome.debugger.onEvent.removeListener(listener);
                resolve();
              }
            } catch (e) {
              chrome.debugger.onEvent.removeListener(listener);
              chrome.test.fail(
                  'Fetch.fulfillRequest failed with: ' +
                  (e.message || String(e)));
              resolve();
            }
          });
    });

    await chrome.debugger.sendCommand(target, 'Fetch.enable', {
      patterns: [
        {urlPattern: restrictedUrl, requestStage: 'Request'},
        {urlPattern: allowedUrl, requestStage: 'Request'},
      ],
    });

    const evalPromise = chrome.debugger.sendCommand(target, 'Runtime.evaluate', {
      expression: `(async () => {
             await fetch('${
          restrictedUrl}', {mode:'no-cors', credentials:'include'}).catch(()=>{});
             await fetch('${
          allowedUrl}', {mode:'no-cors', credentials:'include'}).catch(()=>{});
           })()`,
      awaitPromise: true,
    });

    await Promise.all([interceptionPromise, evalPromise]);

    chrome.test.succeed();
  }
]);

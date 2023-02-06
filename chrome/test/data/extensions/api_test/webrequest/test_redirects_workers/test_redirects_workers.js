// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async() => {
  const scriptUrl = '_test_resources/api_test/webrequest/framework.js';
  await chrome.test.loadScript(scriptUrl);
  const workerJsContent = await (await fetch('page/worker.js')).text();
  const config = await new Promise(resolve => chrome.test.getConfig(resolve));
  const args = JSON.parse(config.customArg);

  const base_url = args.base_url;
  const workerUrl = base_url + 'worker.js';
  const dataWorkerUrl = 'data:text/javascript,' + workerJsContent;
  const redirectWorkerUrl = base_url + 'redirect_worker.js';
  const redirectDataWorkerUrl = base_url + 'redirect_data_worker.js';
  const importRedirectWorkerUrl = base_url + 'import_redirect_worker.js';
  const importRedirectDataWorkerUrl =
      base_url + 'import_redirect_data_worker.js';

  const registerErrorMessage = (url, message) =>
    `Error: Failed to register a ServiceWorker for scope ` +
    `('${base_url}') with script ('${url}'): ${message}`;

  const runSubTest = (workerClass, url, subresourceUrl, expected) => {
    const testDocumentUrl = new URL(base_url + 'test.html');
    testDocumentUrl.searchParams.set('workerClass', workerClass);
    testDocumentUrl.searchParams.set('workerUrl', url);
    if (subresourceUrl) {
      testDocumentUrl.searchParams.set('subresourceUrl', subresourceUrl);
    }

    const listener = () => { return {redirectUrl: workerUrl}; };
    const listenerDataUrl = () => { return {redirectUrl: dataWorkerUrl}; };
    chrome.webRequest.onBeforeRequest.addListener(listener,
        {urls: [redirectWorkerUrl]}, ['blocking']);
    chrome.webRequest.onBeforeRequest.addListener(listenerDataUrl,
        {urls: [redirectDataWorkerUrl]}, ['blocking']);

    navigateAndWait(testDocumentUrl, tab => {
      const messageListener = chrome.test.callbackPass(r => {
        chrome.webRequest.onBeforeRequest.removeListener(listener);
        chrome.webRequest.onBeforeRequest.removeListener(listenerDataUrl);
        chrome.runtime.onMessage.removeListener(messageListener);
        chrome.test.assertEq(expected, r.status);
      });
      chrome.runtime.onMessage.addListener(messageListener);
      chrome.tabs.executeScript(tab.id, {
          runAt: 'document_end',
          code: `(async () => {
              const elem = document.getElementById('status');
              let observer;
              const check = () => {
                const status = elem.textContent;
                if (status === 'not set') {
                  return;
                }
                chrome.runtime.sendMessage({status});
                observer.disconnect();
              };
              observer = new MutationObserver(check);
              observer.observe(elem, {childList: true});
              check();
            })();`
      });
    });
  };


  runTests([
    // HTTP(S)->HTTP(S) redirects for top-level scripts.
    function redirectForWorkerToplevelScript() {
      runSubTest('Worker', redirectWorkerUrl, null, workerUrl);
    },
    function redirectForSharedWorkerToplevelScript() {
      runSubTest('SharedWorker', redirectWorkerUrl, null, workerUrl);
    },
    function redirectForServiceWorkerToplevelScript() {
      // Redirects are disallowed for service worker top-level scripts.
      runSubTest(
          'ServiceWorker', redirectWorkerUrl, null,
          registerErrorMessage(
              redirectWorkerUrl,
              'The script resource is behind a redirect, which is disallowed.')
      );
    },

    // HTTP(S)->data: URL redirects for top-level scripts.
    // They are considered cross-origin redirects and thus disallowed in worker
    // top-level scripts general.
    function redirectToDataUrlForWorkerToplevelScript() {
      runSubTest('Worker', redirectDataWorkerUrl, null, 'Error: undefined');
    },
    function redirectToDataUrlForSharedWorkerToplevelScript() {
      runSubTest('SharedWorker', redirectDataWorkerUrl, null,
                 'Error: undefined');
    },
    function redirectToDataUrlForServiceWorkerToplevelScript() {
      // Redirects are disallowed for service worker top-level scripts.
      runSubTest(
          'ServiceWorker', redirectDataWorkerUrl, null,
          registerErrorMessage(
              redirectDataWorkerUrl,
              'The script resource is behind a redirect, which is disallowed.')
      );
    },

    // HTTP(S)->HTTP(S) redirects for `importScripts()`.
    function redirectForWorkerImportScripts() {
      runSubTest('Worker', importRedirectWorkerUrl, null,
                 importRedirectWorkerUrl);
    },
    function redirectForSharedWorkerImportScripts() {
      runSubTest('SharedWorker', importRedirectWorkerUrl, null,
                 importRedirectWorkerUrl);
    },
    function redirectForServiceWorkerImportScripts() {
      // Redirects are currently disallowed for importScripts() in service
      // workers on Chrome, but at least non-extension HTTP redirects
      // should be allowed (https://crbug.com/889798).
      runSubTest('ServiceWorker', importRedirectWorkerUrl, null,
                 registerErrorMessage(
                     importRedirectWorkerUrl,
                     'ServiceWorker script evaluation failed'));
    },

    // HTTP(S)->data: URL redirects for `importScripts()`.
    function redirectToDataUrlForWorkerImportScripts() {
      runSubTest('Worker', importRedirectDataWorkerUrl, null,
                 importRedirectDataWorkerUrl);
    },
    function redirectToDataUrlForSharedWorkerImportScripts() {
      runSubTest('SharedWorker', importRedirectDataWorkerUrl, null,
                 importRedirectDataWorkerUrl);
    },
    function redirectForServiceWorkerImportScripts() {
      runSubTest('ServiceWorker', importRedirectDataWorkerUrl, null,
                 registerErrorMessage(
                     importRedirectDataWorkerUrl,
                     'ServiceWorker script evaluation failed'));
    },

    // HTTP(S)->HTTP(S) redirects for subresources.
    // `redirectWorkerUrl` and `redirectDataWorkerUrl` below are used as
    // subresource URLs for Fetch API.
    function redirectForWorkerSubresource() {
      runSubTest('Worker', workerUrl, redirectWorkerUrl, workerUrl);
    },
    function redirectForSharedWorkerSubresource() {
      runSubTest('SharedWorker', workerUrl, redirectWorkerUrl, workerUrl);
    },
    function redirectForServiceWorkerSubresource() {
      runSubTest('ServiceWorker', workerUrl, redirectWorkerUrl, workerUrl);
    },

    // HTTP(S)->data: URL redirects for subresources.
    function redirectToDataUrlForWorkerSubresource() {
      runSubTest('Worker', workerUrl, redirectDataWorkerUrl, workerUrl);
    },
    function redirectToDataUrlForSharedWorkerSubresource() {
      runSubTest('SharedWorker', workerUrl, redirectDataWorkerUrl, workerUrl);
    },
    function redirectToDataUrlForServiceWorkerSubresource() {
      runSubTest('ServiceWorker', workerUrl, redirectDataWorkerUrl, workerUrl);
    },
  ]);
})();

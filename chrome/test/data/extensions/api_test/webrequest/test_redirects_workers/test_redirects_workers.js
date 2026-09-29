// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async () => {
  const scriptUrl = '_test_resources/api_test/webrequest/framework.js';
  await chrome.test.loadScript(scriptUrl);
  const workerJsContent = await (await fetch('page/worker.js')).text();
  const config = await new Promise(resolve => chrome.test.getConfig(resolve));
  const args = JSON.parse(config.customArg);

  const baseUrl = args.base_url;
  const workerUrl = `${baseUrl}worker.js`;
  const dataWorkerUrl = `data:text/javascript,${workerJsContent}`;
  const extensionWorkerUrl = chrome.runtime.getURL('page/worker.js');
  const redirectWorkerUrl = `${baseUrl}redirect_worker.js`;
  const redirectDataWorkerUrl = `${baseUrl}redirect_data_worker.js`;
  const redirectExtensionWorkerUrl = `${baseUrl}redirect_extension_worker.js`;
  const importRedirectWorkerUrl = `${baseUrl}import_redirect_worker.js`;
  const importRedirectDataWorkerUrl =
      `${baseUrl}import_redirect_data_worker.js`;
  const importRedirectExtensionWorkerUrl =
      `${baseUrl}import_redirect_extension_worker.js`;

  const registerErrorMessage = (url, message) =>
      `Error: Failed to register a ServiceWorker for scope ` +
      `('${baseUrl}') with script ('${url}'): ${message}`;

  // Maps each URL that is redirected by the `onBeforeRequest` listeners in
  // `runSubTest()` to the URL it is redirected to.
  const redirectTargets = new Map([
    [redirectWorkerUrl, workerUrl],
    [redirectDataWorkerUrl, dataWorkerUrl],
    [redirectExtensionWorkerUrl, extensionWorkerUrl],
  ]);

  // Maps each worker script that calls `importScripts()` to the redirected URL
  // it imports.
  const importedUrls = new Map([
    [importRedirectWorkerUrl, redirectWorkerUrl],
    [importRedirectDataWorkerUrl, redirectDataWorkerUrl],
    [importRedirectExtensionWorkerUrl, redirectExtensionWorkerUrl],
  ]);

  // Returns whether `actualRedirectUrl` reported by `onBeforeRedirect` matches
  // `expectedRedirectUrl`. `data:` URLs are only compared by prefix since the
  // URL parser strips characters (e.g. newlines) from the script contents.
  const redirectUrlMatches = (actualRedirectUrl, expectedRedirectUrl) => {
    if (expectedRedirectUrl.startsWith('data:')) {
      return actualRedirectUrl.startsWith('data:text/javascript,');
    }
    return actualRedirectUrl === expectedRedirectUrl;
  };

  const runSubTest = (workerClass, url, subresourceUrl, expected) => {
    const testDocumentUrl = new URL(`${baseUrl}test.html`);
    testDocumentUrl.searchParams.set('workerClass', workerClass);
    testDocumentUrl.searchParams.set('workerUrl', url);
    if (subresourceUrl) {
      testDocumentUrl.searchParams.set('subresourceUrl', subresourceUrl);
    }

    // Determine which request in this subtest is expected to be redirected:
    // the subresource, the script imported via `importScripts()`, or the
    // top-level worker script.
    const redirectedUrl = subresourceUrl ?? importedUrls.get(url) ?? url;
    const expectedRedirectUrl = redirectTargets.get(redirectedUrl);
    chrome.test.assertTrue(
        !!expectedRedirectUrl, `No redirect is set up for ${redirectedUrl}`);

    // Wait for `onBeforeRedirect` to confirm that the redirect actually
    // happened. Otherwise the subtest could pass even when the redirect never
    // happens because a 404 for `redirectedUrl` often leads to the same result.
    let onBeforeRedirectListener;
    const redirectObserved = new Promise(resolve => {
      onBeforeRedirectListener = details => resolve(details.redirectUrl);
    });
    chrome.webRequest.onBeforeRedirect.addListener(
        onBeforeRedirectListener, {urls: [redirectedUrl]});
    redirectObserved.then(chrome.test.callbackPass(actualRedirectUrl => {
      chrome.webRequest.onBeforeRedirect.removeListener(
          onBeforeRedirectListener);
      chrome.test.assertTrue(
          redirectUrlMatches(actualRedirectUrl, expectedRedirectUrl),
          `${redirectedUrl} was redirected to ${actualRedirectUrl} instead ` +
              `of ${expectedRedirectUrl}`);
    }));

    const listener = () => {
      return {redirectUrl: workerUrl};
    };
    const listenerDataUrl = () => {
      return {redirectUrl: dataWorkerUrl};
    };
    const listenerExtensionUrl = () => {
      return {redirectUrl: extensionWorkerUrl};
    };
    chrome.webRequest.onBeforeRequest.addListener(
        listener, {urls: [redirectWorkerUrl]}, ['blocking']);
    chrome.webRequest.onBeforeRequest.addListener(
        listenerDataUrl, {urls: [redirectDataWorkerUrl]}, ['blocking']);
    chrome.webRequest.onBeforeRequest.addListener(
        listenerExtensionUrl, {urls: [redirectExtensionWorkerUrl]},
        ['blocking']);

    navigateAndWait(testDocumentUrl, tab => {
      const messageListener = chrome.test.callbackPass(r => {
        chrome.webRequest.onBeforeRequest.removeListener(listener);
        chrome.webRequest.onBeforeRequest.removeListener(listenerDataUrl);
        chrome.webRequest.onBeforeRequest.removeListener(listenerExtensionUrl);
        chrome.runtime.onMessage.removeListener(messageListener);
        chrome.test.assertEq(expected, r.status);
      });
      chrome.runtime.onMessage.addListener(messageListener);
      chrome.scripting.executeScript({
        target: {tabId: tab.id},
        func: () => {
          const elem = document.getElementById('status');
          const check = () => {
            const status = elem.textContent;
            if (status === 'not set') {
              return;
            }
            chrome.runtime.sendMessage({status});
            observer.disconnect();
          };
          const observer = new MutationObserver(check);
          observer.observe(
              elem, {childList: true, characterData: true, subtree: true});
          check();
        },
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
          'ServiceWorker',
          redirectWorkerUrl,
          null,
          registerErrorMessage(
              redirectWorkerUrl,
              'The script resource is behind a redirect, which is disallowed.'),
      );
    },

    // HTTP(S)->data: URL redirects for top-level scripts.
    // They are considered cross-origin redirects and thus disallowed in worker
    // top-level scripts general.
    function redirectToDataUrlForWorkerToplevelScript() {
      runSubTest('Worker', redirectDataWorkerUrl, null, 'Error: undefined');
    },
    function redirectToDataUrlForSharedWorkerToplevelScript() {
      runSubTest(
          'SharedWorker', redirectDataWorkerUrl, null, 'Error: undefined');
    },
    function redirectToDataUrlForServiceWorkerToplevelScript() {
      // Redirects are disallowed for service worker top-level scripts.
      runSubTest(
          'ServiceWorker',
          redirectDataWorkerUrl,
          null,
          registerErrorMessage(
              redirectDataWorkerUrl,
              'The script resource is behind a redirect, which is disallowed.'),
      );
    },

    // HTTP(S)->chrome-extension:// URL redirects for top-level scripts.
    // They are considered cross-origin redirects and thus disallowed in worker
    // top-level scripts.
    function redirectToExtensionUrlForWorkerToplevelScript() {
      runSubTest(
          'Worker', redirectExtensionWorkerUrl, null, 'Error: undefined');
    },
    function redirectToExtensionUrlForSharedWorkerToplevelScript() {
      runSubTest(
          'SharedWorker', redirectExtensionWorkerUrl, null, 'Error: undefined');
    },
    function redirectToExtensionUrlForServiceWorkerToplevelScript() {
      // Redirects are disallowed for service worker top-level scripts.
      runSubTest(
          'ServiceWorker',
          redirectExtensionWorkerUrl,
          null,
          registerErrorMessage(
              redirectExtensionWorkerUrl,
              'The script resource is behind a redirect, which is disallowed.'),
      );
    },

    // HTTP(S)->HTTP(S) redirects for `importScripts()`.
    function redirectForWorkerImportScripts() {
      runSubTest(
          'Worker', importRedirectWorkerUrl, null, importRedirectWorkerUrl);
    },
    function redirectForSharedWorkerImportScripts() {
      runSubTest(
          'SharedWorker', importRedirectWorkerUrl, null,
          importRedirectWorkerUrl);
    },
    function redirectForServiceWorkerImportScripts() {
      // Redirects are currently disallowed for importScripts() in service
      // workers on Chrome, but at least non-extension HTTP redirects
      // should be allowed (https://crbug.com/40595655).
      runSubTest(
          'ServiceWorker', importRedirectWorkerUrl, null,
          registerErrorMessage(
              importRedirectWorkerUrl,
              'ServiceWorker script evaluation failed'));
    },

    // HTTP(S)->data: URL redirects for `importScripts()`.
    function redirectToDataUrlForWorkerImportScripts() {
      runSubTest(
          'Worker', importRedirectDataWorkerUrl, null,
          importRedirectDataWorkerUrl);
    },
    function redirectToDataUrlForSharedWorkerImportScripts() {
      runSubTest(
          'SharedWorker', importRedirectDataWorkerUrl, null,
          importRedirectDataWorkerUrl);
    },
    function redirectForServiceWorkerImportScripts() {
      runSubTest(
          'ServiceWorker', importRedirectDataWorkerUrl, null,
          registerErrorMessage(
              importRedirectDataWorkerUrl,
              'ServiceWorker script evaluation failed'));
    },

    // HTTP(S)->chrome-extension:// URL redirects for `importScripts()`.
    function redirectToExtensionUrlForWorkerImportScripts() {
      runSubTest(
          'Worker', importRedirectExtensionWorkerUrl, null,
          importRedirectExtensionWorkerUrl);
    },
    function redirectToExtensionUrlForSharedWorkerImportScripts() {
      runSubTest(
          'SharedWorker', importRedirectExtensionWorkerUrl, null,
          importRedirectExtensionWorkerUrl);
    },
    function redirectToExtensionUrlForServiceWorkerImportScripts() {
      runSubTest(
          'ServiceWorker', importRedirectExtensionWorkerUrl, null,
          registerErrorMessage(
              importRedirectExtensionWorkerUrl,
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

    // HTTP(S)->chrome-extension:// URL redirects for subresources.
    function redirectToExtensionUrlForWorkerSubresource() {
      runSubTest('Worker', workerUrl, redirectExtensionWorkerUrl, workerUrl);
    },
    function redirectToExtensionUrlForSharedWorkerSubresource() {
      runSubTest(
          'SharedWorker', workerUrl, redirectExtensionWorkerUrl, workerUrl);
    },
    function redirectToExtensionUrlForServiceWorkerSubresource() {
      runSubTest(
          'ServiceWorker', workerUrl, redirectExtensionWorkerUrl, workerUrl);
    },
  ]);
})();

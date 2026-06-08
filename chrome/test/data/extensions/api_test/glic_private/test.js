// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

(async () => {
  const config = await chrome.test.getConfig();
  const mode = config.customArg || 'fully_enabled';

  let origin = 'example.com';
  let path = '/simple.html';
  if (mode === 'account_mismatch') {
    origin = 'gemini.google.com';
    path = '/simple.html?authuser=1';
  }

  // Navigates to an url requested by the extension and returns the opened tab.
  async function navigateTo(origin, path) {
    const config = await chrome.test.getConfig();
    const url = `http://${origin}:${config.testServer.port}${path}`;
    return await openTab(url);
  }

  const newTab = await navigateTo(origin, path);
  const frame =
      await chrome.webNavigation.getFrame({frameId: 0, tabId: newTab.id});
  const documentId = frame.documentId;

  if (!documentId) {
    chrome.test.fail('documentId is required for invoke tests');
  }

  switch (mode) {
    case 'fully_enabled':
      tests_runFullyEnabled(documentId);
      return;
    case 'disabled':
      tests_runDisabled(documentId);
      return;
    case 'not_ready':
      tests_runNotReady(documentId);
      return;
    case 'feature_disabled':
    case 'invoke_feature_disabled':
      tests_runFeatureDisabled(documentId);
      return;
    case 'invoke':
    case 'invoke_not_ready':
      tests_runInvoke(documentId);
      return;
    case 'invoke_disabled':
      tests_runInvokeDisabled(documentId);
      return;
    case 'invoke_new_tab':
      tests_runInvokeNewTab(documentId);
      return;
    case 'invoke_server_error':
      tests_runInvokeServerError(documentId);
      return;
    case 'universal_cart_only':
      tests_runUniversalCartOnly(documentId);
      return;
    case 'promotion_page_only':
      tests_runPromotionPageOnly(documentId);
      return;
    case 'both_access_disabled':
      tests_runBothAccessDisabled(documentId);
      return;
    case 'account_mismatch':
      tests_runAccountMismatch(documentId);
      return;
    default:
      chrome.test.fail('invalid mode provided.');
      return;
  }
})();

function tests_runFullyEnabled(documentId) {
  chrome.test.runTests([async function getState() {
    const state = await chrome.glicPrivate.getState(documentId);
    chrome.test.assertNoLastError();
    chrome.test.assertTrue(!!state);

    chrome.test.assertTrue(state.isEnabled, 'isEnabled should be true');
    chrome.test.assertTrue(
        state.isEnabledAndConsented, 'isEnabledAndConsented should be true');
    chrome.test.assertEq('ready', state.readyState);

    chrome.test.assertTrue(state.liveAllowed, 'liveAllowed should be true');
    chrome.test.assertTrue(
        state.shareImageAllowed, 'shareImageAllowed should be true');
    chrome.test.assertTrue(
        state.actuationAllowed, 'actuationAllowed should be true');

    chrome.test.succeed();
  }]);
}

function tests_runDisabled(documentId) {
  chrome.test.runTests([async function getState() {
    const state = await chrome.glicPrivate.getState(documentId);
    chrome.test.assertNoLastError();
    chrome.test.assertTrue(!!state);

    chrome.test.assertFalse(state.isEnabled, 'isEnabled should be false');
    chrome.test.assertFalse(
        state.isEnabledAndConsented, 'isEnabledAndConsented should be false');
    chrome.test.assertEq(
        'ineligible', state.readyState, 'readyState should be INELIGIBLE');

    chrome.test.assertFalse(state.liveAllowed, 'liveAllowed should be false');
    chrome.test.assertFalse(
        state.shareImageAllowed, 'shareImageAllowed should be false');
    // Note: actuation can be allowed by policy even if glic is
    // not otherwise enabled.
    chrome.test.assertTrue(
        state.actuationAllowed, 'actuationAllowed should be true');
    chrome.test.succeed();
  }]);
}

function tests_runNotReady(documentId) {
  chrome.test.runTests([async function getState() {
    const state = await chrome.glicPrivate.getState(documentId);
    chrome.test.assertNoLastError();
    chrome.test.assertTrue(!!state);

    chrome.test.assertTrue(state.isEnabled, 'isEnabled should be true');
    // Not ready because FRE is not completed.
    chrome.test.assertFalse(
        state.isEnabledAndConsented, 'isEnabledAndConsented should be false');

    // These are true because the account capability is still present.
    chrome.test.assertTrue(state.liveAllowed, 'liveAllowed should be true');
    chrome.test.assertTrue(
        state.shareImageAllowed, 'shareImageAllowed should be true');
    chrome.test.assertTrue(
        state.actuationAllowed, 'actuationAllowed should be true');
    chrome.test.succeed();
  }]);
}

function tests_runFeatureDisabled(documentId) {
  chrome.test.runTests([function isUndefined() {
    chrome.test.assertEq(undefined, chrome.glicPrivate);
    chrome.test.succeed();
  }]);
}

function tests_runInvoke(documentId) {
  chrome.test.runTests([
    async function invokeUniversalCartSuccess() {
      await chrome.glicPrivate.invoke({
        promptId: 'TEST_PROMPT_ID',
        invocationSource: chrome.glicPrivate.InvocationSource.UNIVERSAL_CART,
        documentId,
      });
      chrome.test.succeed();
    },
    async function invokePromotionPageSuccess() {
      await chrome.glicPrivate.invoke({
        invocationSource: chrome.glicPrivate.InvocationSource.PROMOTION_PAGE,
        documentId,
      });
      chrome.test.succeed();
    },
  ]);
}

function tests_runInvokeDisabled(documentId) {
  chrome.test.runTests([async function invoke() {
    await chrome.test.assertPromiseRejects(
        chrome.glicPrivate.invoke({
          promptId: 'TEST_PROMPT_ID',
          invocationSource: chrome.glicPrivate.InvocationSource.UNIVERSAL_CART,
          documentId,
        }),
        'Error: local-glic-not-enabled');
    chrome.test.succeed();
  }]);
}

function tests_runInvokeNewTab(documentId) {
  chrome.test.runTests([async function invokeSuccessInNewTab() {
    await chrome.glicPrivate.invoke({
      promptId: 'TEST_PROMPT_ID',
      invocationSource: chrome.glicPrivate.InvocationSource.UNIVERSAL_CART,
      inNewTab: true,
      documentId,
    });
    chrome.test.succeed();
  }]);
}

function tests_runInvokeNotReady(documentId) {
  chrome.test.runTests([async function invoke() {
    await chrome.glicPrivate.invoke({
      promptId: 'TEST_PROMPT_ID',
      invocationSource: chrome.glicPrivate.InvocationSource.UNIVERSAL_CART,
      documentId,
    });
    // Invoke should work even if not consented, as it triggers FRE.
    chrome.test.succeed();
  }]);
}

function tests_runAccountMismatch(documentId) {
  chrome.test.runTests([async function getState() {
    await chrome.test.assertPromiseRejects(
        chrome.glicPrivate.getState(documentId), /local-account-mismatch/);
    chrome.test.succeed();
  }]);
}

function tests_runInvokeServerError(documentId) {
  chrome.test.runTests([
    async function invokeHttpError() {
      await chrome.test.assertPromiseRejects(
          chrome.glicPrivate.invoke({
            promptId: 'http_error',
            invocationSource:
                chrome.glicPrivate.InvocationSource.UNIVERSAL_CART,
            documentId,
          }),
          'Error: http-error');
      chrome.test.succeed();
    },
  ]);
}

function tests_runUniversalCartOnly(documentId) {
  chrome.test.runTests([
    async function invokeUniversalCartSuccess() {
      await chrome.glicPrivate.invoke({
        promptId: 'TEST_PROMPT_ID',
        invocationSource: chrome.glicPrivate.InvocationSource.UNIVERSAL_CART,
        documentId,
      });
      chrome.test.succeed();
    },
    async function invokePromotionPageDisabled() {
      await chrome.test.assertPromiseRejects(
          chrome.glicPrivate.invoke({
            invocationSource:
                chrome.glicPrivate.InvocationSource.PROMOTION_PAGE,
            documentId,
          }),
          'Error: local-glic-access-from-page-disabled');
      chrome.test.succeed();
    },
  ]);
}

function tests_runPromotionPageOnly(documentId) {
  chrome.test.runTests([
    async function invokeUniversalCartDisabled() {
      await chrome.test.assertPromiseRejects(
          chrome.glicPrivate.invoke({
            promptId: 'TEST_PROMPT_ID',
            invocationSource:
                chrome.glicPrivate.InvocationSource.UNIVERSAL_CART,
            documentId,
          }),
          'Error: local-glic-access-from-page-disabled');
      chrome.test.succeed();
    },
    async function invokePromotionPageSuccess() {
      await chrome.glicPrivate.invoke({
        invocationSource: chrome.glicPrivate.InvocationSource.PROMOTION_PAGE,
        documentId,
      });
      chrome.test.succeed();
    },
  ]);
}

function tests_runBothAccessDisabled(documentId) {
  chrome.test.runTests([
    async function invokeUniversalCartDisabled() {
      await chrome.test.assertPromiseRejects(
          chrome.glicPrivate.invoke({
            promptId: 'TEST_PROMPT_ID',
            invocationSource:
                chrome.glicPrivate.InvocationSource.UNIVERSAL_CART,
            documentId,
          }),
          'Error: local-glic-access-from-page-disabled');
      chrome.test.succeed();
    },
    async function invokePromotionPageDisabled() {
      await chrome.test.assertPromiseRejects(
          chrome.glicPrivate.invoke({
            invocationSource:
                chrome.glicPrivate.InvocationSource.PROMOTION_PAGE,
            documentId,
          }),
          'Error: local-glic-access-from-page-disabled');
      chrome.test.succeed();
    },
  ]);
}

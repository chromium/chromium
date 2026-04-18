// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(config => {
  const mode = config.customArg || 'fully_enabled';

  switch (mode) {
    case 'fully_enabled':
      tests_runFullyEnabled();
      return;
    case 'disabled':
      tests_runDisabled();
      return;
    case 'not_ready':
      tests_runNotReady();
      return;
    case 'feature_disabled':
    case 'invoke_feature_disabled':
      tests_runFeatureDisabled();
      return;
    case 'invoke':
    case 'invoke_not_ready':
      tests_runInvoke();
      return;
    case 'invoke_disabled':
      tests_runInvokeDisabled();
      return;
    case 'invoke_new_tab':
      tests_runInvokeNewTab();
      return;
    case 'invoke_server_error':
      tests_runInvokeServerError();
      return;
    default:
      chrome.test.fail('invalid mode provided.');
      return;
  }
});

function tests_runFullyEnabled() {
  chrome.test.runTests([async function getState() {
    const state = await chrome.glicPrivate.getState();
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

function tests_runDisabled() {
  chrome.test.runTests([async function getState() {
    const state = await chrome.glicPrivate.getState();
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

function tests_runNotReady() {
  chrome.test.runTests([async function getState() {
    const state = await chrome.glicPrivate.getState();
    chrome.test.assertNoLastError();
    chrome.test.assertTrue(!!state);

    chrome.test.assertTrue(state.isEnabled, 'isEnabled should be true');
    // Not ready because FRE is not completed.
    chrome.test.assertFalse(
        state.isEnabledAndConsented, 'isEnabledAndConsented should be false');

    // Ready state can change depending on the value of FRE flags, so don't
    // test it specifically here.

    // These are true because the account capability is still present.
    chrome.test.assertTrue(state.liveAllowed, 'liveAllowed should be true');
    chrome.test.assertTrue(
        state.shareImageAllowed, 'shareImageAllowed should be true');
    chrome.test.assertTrue(
        state.actuationAllowed, 'actuationAllowed should be true');
    chrome.test.succeed();
  }]);
}

function tests_runFeatureDisabled() {
  chrome.test.runTests([function isUndefined() {
    chrome.test.assertEq(undefined, chrome.glicPrivate);
    chrome.test.succeed();
  }]);
}

function tests_runInvoke() {
  chrome.test.runTests([async function invokeSuccess() {
    await chrome.glicPrivate.invoke({
      promptId: 'TEST_PROMPT_ID',
      invocationSource: chrome.glicPrivate.InvocationSource.UNIVERSAL_CART
    });
    chrome.test.succeed();
  }]);
}

function tests_runInvokeDisabled() {
  chrome.test.runTests([async function invoke() {
    await chrome.test.assertPromiseRejects(
        chrome.glicPrivate.invoke({
          promptId: 'TEST_PROMPT_ID',
          invocationSource: chrome.glicPrivate.InvocationSource.UNIVERSAL_CART
        }),
        'Error: local-glic-not-enabled');
    chrome.test.succeed();
  }]);
}

function tests_runInvokeNewTab() {
  chrome.test.runTests([async function invokeSuccessInNewTab() {
    await chrome.glicPrivate.invoke({
      promptId: 'TEST_PROMPT_ID',
      invocationSource: chrome.glicPrivate.InvocationSource.UNIVERSAL_CART,
      inNewTab: true
    });
    chrome.test.succeed();
  }]);
}

function tests_runInvokeServerError() {
  chrome.test.runTests([
    async function invokeHttpError() {
      await chrome.test.assertPromiseRejects(
          chrome.glicPrivate.invoke({
            promptId: 'http_error',
            invocationSource: chrome.glicPrivate.InvocationSource.UNIVERSAL_CART
          }),
          'Error: http-error');
      chrome.test.succeed();
    },
  ]);
}

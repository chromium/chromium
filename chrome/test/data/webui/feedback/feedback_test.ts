// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FeedbackBrowserProxyImpl} from 'chrome://feedback/js/feedback_browser_proxy.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestFeedbackBrowserProxy} from './test_feedback_browser_proxy.js';

declare global {
  interface Window {
    whenTestSetupDoneResolver: {resolve: () => Promise<void>};
  }
}

suite('FeedbackTest', function() {
  let browserProxy: TestFeedbackBrowserProxy;

  suiteSetup(function() {
    const whenReadyForTesting =
        eventToPromise('ready-for-testing', document.documentElement);

    // Signal to the prod page that test setup steps have completed.
    browserProxy = new TestFeedbackBrowserProxy();
    FeedbackBrowserProxyImpl.setInstance(browserProxy);
    window.whenTestSetupDoneResolver.resolve();

    // Wait for any remaining prod code executes before test cases execute.
    return whenReadyForTesting;
  });

  teardown(function() {
    browserProxy.reset();

    // Note: The UI is not recreated between tests, so must clear any state that
    // could leak between tests here.
  });

  test('CloseButtonClosesDialog', function() {
    const button = getRequiredElement('cancel-button');
    button.click();
    return browserProxy.whenCalled('closeDialog');
  });
});

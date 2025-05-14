// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-base-dialog/base_dialog_app.js';
import 'chrome://privacy-sandbox-base-dialog/topics_consent.js';

import type {BaseDialogApp} from 'chrome://privacy-sandbox-base-dialog/base_dialog_app.js';
import {BaseDialogBrowserProxy} from 'chrome://privacy-sandbox-base-dialog/base_dialog_browser_proxy.js';
import {PrivacySandboxNotice, PrivacySandboxNoticeEvent} from 'chrome://privacy-sandbox-base-dialog/notice.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import type {TestBaseDialogPageHandler} from './test_base_dialog_browser_proxy.js';
import {TestBaseDialogBrowserProxy} from './test_base_dialog_browser_proxy.js';

suite('BaseDialogTest', function() {
  let page: BaseDialogApp;
  let testHandler: TestBaseDialogPageHandler;

  // TODO(crbug.com/398005782): Since this loadTimeData value is hard coded in
  // BaseDialogUI, we set that here. Once that is removed, we can refactor this
  // logic to test all of the other notices.
  suiteSetup(function() {
    loadTimeData.overrideValues({
      noticeIdToShow: PrivacySandboxNotice.kTopicsConsentNotice,
    });
  });

  setup(async function() {
    const testBrowserProxy = new TestBaseDialogBrowserProxy();
    BaseDialogBrowserProxy.setInstance(testBrowserProxy);
    testHandler = testBrowserProxy.handler;

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('base-dialog-app');
    document.body.appendChild(page);

    await testHandler.whenCalled('resizeDialog');
    await testHandler.whenCalled('showDialog');
    await page.updateComplete;
  });

  test('ShowsConsentView', function() {
    const viewManager = page.shadowRoot.querySelector('cr-view-manager');
    assertTrue(!!viewManager);
    const activeView = viewManager.querySelector('[slot="view"].active');
    assertTrue(!!activeView);
    assertEquals(
        PrivacySandboxNotice[PrivacySandboxNotice.kTopicsConsentNotice],
        activeView.id);
  });

  test('Consent', async function() {
    const topicsConsent = page.shadowRoot.querySelector('topics-consent');
    assertTrue(!!topicsConsent);
    assertTrue(!!topicsConsent.shadowRoot);
    const consentButton =
        topicsConsent.shadowRoot.querySelector<HTMLElement>('#consentButton');
    assertTrue(!!consentButton);
    consentButton.click();
    await testHandler.eventOccurred(
        PrivacySandboxNotice.kTopicsConsentNotice,
        PrivacySandboxNoticeEvent.kOptIn);
    await testHandler.whenCalled('closeDialog');
  });
});

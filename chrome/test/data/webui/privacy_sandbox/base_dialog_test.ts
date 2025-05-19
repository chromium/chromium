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

function testCrViewManager(page: BaseDialogApp, notice: PrivacySandboxNotice) {
  const viewManager = page.shadowRoot.querySelector('cr-view-manager');
  assertTrue(!!viewManager);
  const activeView = viewManager.querySelector('[slot="view"].active');
  assertTrue(!!activeView);
  assertEquals(PrivacySandboxNotice[notice], activeView.id);
}

async function setupBaseDialogApp():
    Promise<{page: BaseDialogApp, testHandler: TestBaseDialogPageHandler}> {
  const testBrowserProxy = new TestBaseDialogBrowserProxy();
  BaseDialogBrowserProxy.setInstance(testBrowserProxy);
  const testHandler = testBrowserProxy.handler;

  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  const page = document.createElement('base-dialog-app');
  document.body.appendChild(page);

  await testHandler.whenCalled('resizeDialog');
  await testHandler.whenCalled('showDialog');
  await page.updateComplete;

  return {page, testHandler};
}

suite('TopicsConsent', function() {
  let page: BaseDialogApp;
  let testHandler: TestBaseDialogPageHandler;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      noticeIdToShow: PrivacySandboxNotice.kTopicsConsentNotice,
    });
  });

  setup(async function() {
    ({page, testHandler} = await setupBaseDialogApp());
  });

  test('CrViewManager', function() {
    testCrViewManager(page, PrivacySandboxNotice.kTopicsConsentNotice);
  });

  test('Consent', async function() {
    const topicsConsent = page.shadowRoot.querySelector('topics-consent');
    assertTrue(!!topicsConsent);
    assertTrue(!!topicsConsent.shadowRoot);
    const acceptButton =
        topicsConsent.shadowRoot.querySelector<HTMLElement>('#acceptButton');
    assertTrue(!!acceptButton);
    acceptButton.click();
    await testHandler.eventOccurred(
        PrivacySandboxNotice.kTopicsConsentNotice,
        PrivacySandboxNoticeEvent.kOptIn);
    await testHandler.whenCalled('closeDialog');
  });
});

suite('ProtectedAudienceMeasurement', function() {
  let page: BaseDialogApp;
  let testHandler: TestBaseDialogPageHandler;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      noticeIdToShow: PrivacySandboxNotice.kProtectedAudienceMeasurementNotice,
    });
  });

  setup(async function() {
    ({page, testHandler} = await setupBaseDialogApp());
  });

  test('CrViewManager', function() {
    testCrViewManager(
        page, PrivacySandboxNotice.kProtectedAudienceMeasurementNotice);
  });

  test('Notice', async function() {
    const protectedAudienceMeasurement =
        page.shadowRoot.querySelector('protected-audience-measurement');
    assertTrue(!!protectedAudienceMeasurement);
    assertTrue(!!protectedAudienceMeasurement.shadowRoot);
    const ackButton =
        protectedAudienceMeasurement.shadowRoot.querySelector<HTMLElement>(
            '#ackButton');
    assertTrue(!!ackButton);
    ackButton.click();
    await testHandler.eventOccurred(
        PrivacySandboxNotice.kProtectedAudienceMeasurementNotice,
        PrivacySandboxNoticeEvent.kAck);
  });
});

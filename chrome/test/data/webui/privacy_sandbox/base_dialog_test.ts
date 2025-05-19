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

function getNoticeComponentSelector(notice: PrivacySandboxNotice) {
  switch (notice) {
    case PrivacySandboxNotice.kTopicsConsentNotice:
      return 'topics-consent';
    case PrivacySandboxNotice.kProtectedAudienceMeasurementNotice:
      return 'protected-audience-measurement';
    case PrivacySandboxNotice.kThreeAdsApisNotice:
      return 'three-ads-apis';
    default:
      return '';
  }
}

function getButtonIdFromEvent(event: PrivacySandboxNoticeEvent) {
  switch (event) {
    case PrivacySandboxNoticeEvent.kOptIn:
      return '#acceptButton';
    case PrivacySandboxNoticeEvent.kAck:
      return '#ackButton';
    default:
      return '';
  }
}

async function testButtonClick(
    page: BaseDialogApp, notice: PrivacySandboxNotice,
    event: PrivacySandboxNoticeEvent, testHandler: TestBaseDialogPageHandler) {
  const noticeComponent =
      page.shadowRoot.querySelector(getNoticeComponentSelector(notice));
  assertTrue(!!noticeComponent);
  assertTrue(!!noticeComponent.shadowRoot);
  const button = noticeComponent.shadowRoot.querySelector<HTMLElement>(
      getButtonIdFromEvent(event));
  assertTrue(!!button);
  button.click();
  await testHandler.eventOccurred(notice, event);
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
    await testButtonClick(
        page, PrivacySandboxNotice.kTopicsConsentNotice,
        PrivacySandboxNoticeEvent.kOptIn, testHandler);
    // TODO(crbug.com/417700269): Remove this once close dialog method is
    // removed from the mojo interface and the View Manager handles closing the
    // dialog.
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
    await testButtonClick(
        page, PrivacySandboxNotice.kProtectedAudienceMeasurementNotice,
        PrivacySandboxNoticeEvent.kAck, testHandler);
  });
});

suite('ThreeAdsApis', function() {
  let page: BaseDialogApp;
  let testHandler: TestBaseDialogPageHandler;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      noticeIdToShow: PrivacySandboxNotice.kThreeAdsApisNotice,
    });
  });

  setup(async function() {
    ({page, testHandler} = await setupBaseDialogApp());
  });

  test('CrViewManager', function() {
    testCrViewManager(page, PrivacySandboxNotice.kThreeAdsApisNotice);
  });

  test('Notice', async function() {
    await testButtonClick(
        page, PrivacySandboxNotice.kThreeAdsApisNotice,
        PrivacySandboxNoticeEvent.kAck, testHandler);
  });
});

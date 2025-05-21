// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-base-dialog/base_dialog_app.js';
import 'chrome://privacy-sandbox-base-dialog/topics_consent_notice.js';
import 'chrome://privacy-sandbox-base-dialog/protected_audience_measurement_notice.js';
import 'chrome://privacy-sandbox-base-dialog/three_ads_apis_notice.js';

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
      return 'topics-consent-notice';
    case PrivacySandboxNotice.kProtectedAudienceMeasurementNotice:
      return 'protected-audience-measurement-notice';
    case PrivacySandboxNotice.kThreeAdsApisNotice:
      return 'three-ads-apis-notice';
    case PrivacySandboxNotice.kMeasurementNotice:
      return 'measurement-notice';
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

suite('TopicsConsentNotice', function() {
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
  });
});

suite('ProtectedAudienceMeasurementNotice', function() {
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

suite('ThreeAdsApisNotice', function() {
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

suite('MeasurementNotice', function() {
  let page: BaseDialogApp;
  let testHandler: TestBaseDialogPageHandler;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      noticeIdToShow: PrivacySandboxNotice.kMeasurementNotice,
    });
  });

  setup(async function() {
    ({page, testHandler} = await setupBaseDialogApp());
  });

  test('CrViewManager', function() {
    testCrViewManager(page, PrivacySandboxNotice.kMeasurementNotice);
  });

  test('Notice', async function() {
    await testButtonClick(
        page, PrivacySandboxNotice.kMeasurementNotice,
        PrivacySandboxNoticeEvent.kAck, testHandler);
  });
});

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://downloads/downloads.js';

import type {DownloadsDangerousDownloadInterstitialElement, PageRemote} from 'chrome://downloads/downloads.js';
import {BrowserProxy, loadTimeData} from 'chrome://downloads/downloads.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestDownloadsProxy} from './test_support.js';

suite('interstitial tests', function() {
  let interstitial: DownloadsDangerousDownloadInterstitialElement;
  let callbackRouterRemote: PageRemote;
  let testDownloadsProxy: TestDownloadsProxy;
  const bypassPromptItemId = 'itemId';
  const displayReferrerUrl = 'https://example.com';
  const trustSiteLineUrl = `I trust the site (${displayReferrerUrl})`;
  const trustSiteLineNoUrl = 'I trust the site';

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testDownloadsProxy = new TestDownloadsProxy();
    callbackRouterRemote = testDownloadsProxy.callbackRouterRemote;
    BrowserProxy.setInstance(testDownloadsProxy);

    interstitial =
        document.createElement('downloads-dangerous-download-interstitial');
    interstitial.trustSiteLine = loadTimeData.getStringF(
        'warningBypassInterstitialSurveyTrustSiteWithUrl', displayReferrerUrl);
    interstitial.bypassPromptItemId = bypassPromptItemId;
    document.body.appendChild(interstitial);
    flush();
  });

  test('trust site line with url displays correctly', function() {
    const trustSiteRadioButton = interstitial.shadowRoot!.querySelector(
        'cr-radio-button[name=TrustSite]');
    assertTrue(!!trustSiteRadioButton);
    assertEquals(trustSiteLineUrl, trustSiteRadioButton.textContent!.trim());
  });

  test('trust site line without url displays correctly', function() {
    interstitial.trustSiteLine = loadTimeData.getString(
        'warningBypassInterstitialSurveyTrustSiteWithoutUrl');

    const trustSiteRadioButton = interstitial.shadowRoot!.querySelector(
        'cr-radio-button[name=TrustSite]');
    assertTrue(!!trustSiteRadioButton);
    assertEquals(trustSiteLineNoUrl, trustSiteRadioButton.textContent!.trim());
  });

  test('clicking back to safety cancels the interstitial', function() {
    assertTrue(interstitial.$.dialog.open);

    let cancelCounter = 0;
    interstitial.addEventListener('cancel', function() {
      cancelCounter++;
    });

    const backToSafetyButton =
        interstitial!.shadowRoot!.querySelector<HTMLElement>(
            '#back-to-safety-button');
    assertTrue(!!backToSafetyButton);
    backToSafetyButton.click();

    assertEquals(1, cancelCounter);
    assertFalse(interstitial.$.dialog.open);
  });

  test(
      'clicking download closes the interstitial with return value',
      async () => {
        assertTrue(interstitial.$.dialog.open);

        let closeCounter = 0;
        interstitial.addEventListener('close', function() {
          closeCounter++;
        });

        const surveyGroup =
            interstitial.shadowRoot!.querySelector('cr-radio-group');
        assertTrue(!!surveyGroup);

        const surveyOptions =
            interstitial.shadowRoot!.querySelectorAll('cr-radio-button');
        assertEquals(3, surveyOptions.length);
        surveyOptions[0]!.click();
        await microtasksFinished();

        const downloadButton =
            interstitial!.shadowRoot!.querySelector<HTMLElement>(
                '#download-button');
        assertTrue(!!downloadButton);
        downloadButton.click();

        assertEquals(1, closeCounter);
        assertFalse(interstitial.$.dialog.open);
        const surveyResponse = interstitial.getSurveyResponse();
        // 1 = DangerousDownloadInterstitialSurveyOptions.kCreatedFile
        assertEquals(1, surveyResponse);
      });

  test('bypassing without survey response returns kNoResponse', function() {
    assertTrue(interstitial.$.dialog.open);

    const downloadButton = interstitial!.shadowRoot!.querySelector<HTMLElement>(
        '#download-button');
    assertTrue(!!downloadButton);
    downloadButton.click();

    assertFalse(interstitial.$.dialog.open);

    const surveyResponse = interstitial.getSurveyResponse();
    // 0 = DangerousDownloadInterstitialSurveyOptions.kNoResponse
    assertEquals(0, surveyResponse);
  });

  test(
      'clicking continue records opens survey and disables itself',
      async () => {
        const continueAnywayButton = interstitial.$.continueAnywayButton;
        assertTrue(!!continueAnywayButton);
        assertFalse(continueAnywayButton.disabled);

        const surveyAndDownloadButton = interstitial!.shadowRoot!.querySelector(
            '#survey-and-download-button-wrapper');
        assertFalse(isVisible(surveyAndDownloadButton));

        continueAnywayButton.click();

        assertTrue(isVisible(surveyAndDownloadButton));
        assertTrue(continueAnywayButton.disabled);

        await callbackRouterRemote.$.flushForTesting();
        flush();

        const openSurveyId = await testDownloadsProxy.handler.whenCalled(
            'recordOpenSurveyOnDangerousInterstitial');
        assertEquals(bypassPromptItemId, openSurveyId);
      });

  test('esc key disabled while interstitial open', function() {
    let cancelCounter = 0;
    // Pressing escape normally triggers a 'cancel' event on HTML dialog
    interstitial.addEventListener('cancel', function() {
      cancelCounter++;
    });
    keyDownOn(interstitial, 0, [], 'Escape');

    assertEquals(0, cancelCounter);
    assertTrue(interstitial.$.dialog.open);
  });
});

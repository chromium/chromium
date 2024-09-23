// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://downloads/downloads.js';

import type {DownloadsDangerousDownloadInterstitialElement, PageRemote} from 'chrome://downloads/downloads.js';
import {BrowserProxy, loadTimeData} from 'chrome://downloads/downloads.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
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
  });

  test('trust site line with url displays correctly', function() {
    const trustSiteRadioButton = interstitial.shadowRoot!.querySelector(
        'cr-radio-button[name=TrustSite]');
    assertTrue(!!trustSiteRadioButton);
    assertEquals(trustSiteLineUrl, trustSiteRadioButton.textContent!.trim());
  });

  test('trust site line without url displays correctly', async () => {
    interstitial.trustSiteLine = loadTimeData.getString(
        'warningBypassInterstitialSurveyTrustSiteWithoutUrl');
    await microtasksFinished();

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
            '#backToSafetyButton');
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
        await microtasksFinished();

        assertTrue(isVisible(surveyAndDownloadButton));
        assertTrue(continueAnywayButton.disabled);

        await callbackRouterRemote.$.flushForTesting();

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

  test('selected radio option is set correctly', async () => {
    const surveyGroup =
        interstitial.shadowRoot!.querySelector('cr-radio-group');
    assertTrue(!!surveyGroup);

    const surveyOptions =
        interstitial.shadowRoot!.querySelectorAll('cr-radio-button');
    assertEquals(3, surveyOptions.length);

    assertTrue(!!surveyOptions[0]);
    surveyOptions[0].click();
    await microtasksFinished();
    assertTrue(surveyOptions[0].checked);

    assertTrue(!!surveyOptions[1]);
    surveyOptions[1].click();
    await microtasksFinished();
    assertTrue(surveyOptions[1].checked);

    assertTrue(!!surveyOptions[2]);
    surveyOptions[2].click();
    await microtasksFinished();
    assertTrue(surveyOptions[2].checked);
  });
});

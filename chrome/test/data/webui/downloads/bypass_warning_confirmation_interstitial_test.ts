// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://downloads/downloads.js';

import type {DownloadsDangerousDownloadInterstitialElement} from 'chrome://downloads/downloads.js';
import {BrowserProxy, loadTimeData} from 'chrome://downloads/downloads.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestDownloadsProxy} from './test_support.js';

suite('interstitial tests', function() {
  let interstitial: DownloadsDangerousDownloadInterstitialElement;
  let testDownloadsProxy: TestDownloadsProxy;
  const displayReferrerUrl = 'https://example.com';
  const trustSiteLineUrl = `I trust the site (${displayReferrerUrl})`;
  const trustSiteLineNoUrl = 'I trust the site';

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testDownloadsProxy = new TestDownloadsProxy();

    BrowserProxy.setInstance(testDownloadsProxy);

    interstitial =
        document.createElement('downloads-dangerous-download-interstitial');
    interstitial.trustSiteLine = loadTimeData.getStringF(
        'warningBypassInterstitialSurveyTrustSiteWithUrl', displayReferrerUrl);
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
});

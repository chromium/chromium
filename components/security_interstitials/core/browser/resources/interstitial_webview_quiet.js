// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: The following chrome:// URLs are not actually fetched at runtime. They
// are handled in
// components/security_interstitials/core/browser/resources:bundle_js, which
// finds the correct files and inlines them.
import {HIDDEN_CLASS, preventDefaultOnPoundLinkClicks, SecurityInterstitialCommandId, sendCommand} from 'chrome://interstitials/common/resources/interstitial_common.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

/**
 * Restores the interstitial content to the initial state if the window size
 * switches to a small view.
 */
function onResize() {
  const mediaQuery = '(max-height:11.25em) and (max-width:18.75em),' +
       '(max-height:18.75em) and (max-width:11.25em),' +
       '(max-height:5em), (max-width:5em)';

  // Check for change in window size.
  if (window.matchMedia(mediaQuery).matches) {
    const hiddenDetails =
        document.querySelector('#details').classList.add(HIDDEN_CLASS);
    document.querySelector('#main-content').classList.remove(HIDDEN_CLASS);
    document.querySelector('#icon').setAttribute(
        'aria-label', loadTimeData.getString('heading'));
  } else {
    document.querySelector('#icon').removeAttribute('aria-label');
  }
}

function initPage() {
  // `loadTimeDataRaw` is injected to the `window` scope from C++.
  loadTimeData.data = window.loadTimeDataRaw;

  const isGiantWebView = loadTimeData.getBoolean('is_giant');
  const interstitialType = loadTimeData.getString('type');
  const safebrowsing = interstitialType === 'SAFEBROWSING';
  const heavyAd = interstitialType === 'HEAVYAD';

  document.body.className = isGiantWebView ? 'giant' : '';

  if (heavyAd) {
    document.body.classList.add('heavy-ad');
  }

  preventDefaultOnPoundLinkClicks();

  document.querySelector('#details-link')
      .addEventListener('click', function(event) {
        const hiddenDetails =
            document.querySelector('#details').classList.toggle(HIDDEN_CLASS);
        document.querySelector('#main-content')
            .classList.toggle(HIDDEN_CLASS, !hiddenDetails);
      });

  if (safebrowsing) {
    document.querySelector('#proceed-link')
        .addEventListener('click', function(event) {
          sendCommand(SecurityInterstitialCommandId.CMD_PROCEED);
        });
  }

  window.addEventListener('resize', onResize);
}

document.addEventListener('DOMContentLoaded', initPage);

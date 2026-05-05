// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {preventDefaultOnPoundLinkClicks, SecurityInterstitialCommandId, sendCommand} from 'chrome://interstitials/common/resources/interstitial_common.js';

function initPage() {
  preventDefaultOnPoundLinkClicks();
  const learnMoreLink = document.querySelector('#learn-more-link');
  if (learnMoreLink) {
    learnMoreLink.addEventListener('click', function() {
      sendCommand(SecurityInterstitialCommandId.CMD_SHOW_MORE_SECTION);
    });
  }

  const backLink = document.querySelector('#back-link');
  if (backLink) {
    backLink.addEventListener('click', function() {
      sendCommand(SecurityInterstitialCommandId.CMD_DONT_PROCEED);
    });
  }
}

document.addEventListener('DOMContentLoaded', initPage);

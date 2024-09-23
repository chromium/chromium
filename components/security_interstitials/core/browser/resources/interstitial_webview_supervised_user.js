// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SecurityInterstitialCommandId, sendCommand} from 'chrome://interstitials/common/resources/interstitial_common.js';

function initPage() {
  const learnMoreLink = document.querySelector('#learn-more-link');
  if (learnMoreLink) {
    learnMoreLink.addEventListener('click', function() {
      sendCommand(SecurityInterstitialCommandId.CMD_OPEN_HELP_CENTER);
    });
  }
}

document.addEventListener('DOMContentLoaded', initPage);

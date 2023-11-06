// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SecurityInterstitialCommandId, sendCommand} from 'chrome://interstitials/common/resources/interstitial_common.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

// Other constants defined in security_interstitial_page.h.
const SB_BOX_CHECKED = 'boxchecked';
const SB_DISPLAY_CHECK_BOX = 'displaycheckbox';

// This sets up the Extended Safe Browsing Reporting opt-in, either for
// reporting malware or invalid certificate chains. Does nothing if the
// interstitial type is not SAFEBROWSING or SSL or CAPTIVE_PORTAL.
export function setupExtendedReportingCheckbox() {
  const interstitialType = loadTimeData.getString('type');
  if (interstitialType !== 'SAFEBROWSING' && interstitialType !== 'SSL' &&
      interstitialType !== 'CAPTIVE_PORTAL') {
    return;
  }

  if (!loadTimeData.getBoolean(SB_DISPLAY_CHECK_BOX)) {
    return;
  }

  const privacyLink = document.querySelector('#privacy-link');
  if (privacyLink) {
    privacyLink.addEventListener('click', function() {
      sendCommand(SecurityInterstitialCommandId.CMD_OPEN_REPORTING_PRIVACY);
      return false;
    });
    privacyLink.addEventListener('mousedown', function() {
      return false;
    });
  }
  document.querySelector('#opt-in-checkbox').checked =
      loadTimeData.getBoolean(SB_BOX_CHECKED);
  document.querySelector('#extended-reporting-opt-in')
      .classList.remove('hidden');

  const billing =
      interstitialType === 'SAFEBROWSING' && loadTimeData.getBoolean('billing');

  let className = 'ssl-opt-in';
  if (interstitialType === 'SAFEBROWSING' && !billing) {
    className = 'safe-browsing-opt-in';
  }

  document.querySelector('#extended-reporting-opt-in').classList.add(className);

  document.querySelector('#body').classList.add(
      'extended-reporting-has-checkbox');

  const whitepaperLink = document.querySelector('#whitepaper-link');
  if (whitepaperLink) {
    whitepaperLink.addEventListener('click', function(event) {
      sendCommand(SecurityInterstitialCommandId.CMD_OPEN_WHITEPAPER);
    });
  }

  const optInCheckbox = document.querySelector('#opt-in-checkbox');
  optInCheckbox.addEventListener('click', function() {
    sendCommand(
        optInCheckbox.checked ? SecurityInterstitialCommandId.CMD_DO_REPORT :
                                SecurityInterstitialCommandId.CMD_DONT_REPORT);
  });
}

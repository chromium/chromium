// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Other constants defined in security_interstitial_page.h.
const SB_BOX_CHECKED = 'boxchecked';
const SB_DISPLAY_CHECK_BOX = 'displaycheckbox';

// This sets up the Extended Safe Browsing Reporting opt-in, either for
// reporting malware or invalid certificate chains. Does nothing if the
// interstitial type is not SAFEBROWSING or SSL or CAPTIVE_PORTAL.
function setupExtendedReportingCheckbox() {
  const interstitialType = loadTimeData.getString('type');
  if (interstitialType !== 'SAFEBROWSING' && interstitialType !== 'SSL' &&
      interstitialType !== 'CAPTIVE_PORTAL') {
    return;
  }

  if (!loadTimeData.getBoolean(SB_DISPLAY_CHECK_BOX)) {
    return;
  }

  if ($('privacy-link')) {
    $('privacy-link').addEventListener('click', function() {
      sendCommand(SecurityInterstitialCommandId.CMD_OPEN_REPORTING_PRIVACY);
      return false;
    });
    $('privacy-link').addEventListener('mousedown', function() {
      return false;
    });
  }
  $('opt-in-checkbox').checked = loadTimeData.getBoolean(SB_BOX_CHECKED);
  $('extended-reporting-opt-in').classList.remove('hidden');

  const billing =
      interstitialType === 'SAFEBROWSING' && loadTimeData.getBoolean('billing');

  let className = 'ssl-opt-in';
  if (interstitialType === 'SAFEBROWSING' && !billing) {
    className = 'safe-browsing-opt-in';
  }

  $('extended-reporting-opt-in').classList.add(className);

  $('body').classList.add('extended-reporting-has-checkbox');

  if ($('whitepaper-link')) {
    $('whitepaper-link').addEventListener('click', function(event) {
      sendCommand(SecurityInterstitialCommandId.CMD_OPEN_WHITEPAPER);
    });
  }

  $('opt-in-checkbox').addEventListener('click', function() {
    sendCommand($('opt-in-checkbox').checked ?
                SecurityInterstitialCommandId.CMD_DO_REPORT :
                SecurityInterstitialCommandId.CMD_DONT_REPORT);
  });
}

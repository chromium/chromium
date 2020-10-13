// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Other constants defined in security_interstitial_page.h.
const SB_DISPLAY_ENHANCED_PROTECTION_MESSAGE =
    'displayEnhancedProtectionMessage';

// This sets up the enhanced protection message.
function setupEnhancedProtectionMessage() {
  const interstitialType = loadTimeData.getString('type');
  if (interstitialType !== 'SAFEBROWSING' && interstitialType !== 'SSL' &&
      interstitialType !== 'CAPTIVE_PORTAL') {
    return;
  }

  if (!loadTimeData.getBoolean(SB_DISPLAY_ENHANCED_PROTECTION_MESSAGE)) {
    return;
  }

  if ($('enhanced-protection-link')) {
    $('enhanced-protection-link').addEventListener('click', function() {
      sendCommand(
          SecurityInterstitialCommandId.CMD_OPEN_ENHANCED_PROTECTION_SETTINGS);
      return false;
    });
  }
  $('enhanced-protection-message').classList.remove('hidden');

  const billing =
      interstitialType === 'SAFEBROWSING' && loadTimeData.getBoolean('billing');

  let className = 'ssl-enhanced-protection-message';
  if (interstitialType === 'SAFEBROWSING' && !billing) {
    className = 'safe-browsing-enhanced-protection-message';
  }

  $('enhanced-protection-message').classList.add(className);
}

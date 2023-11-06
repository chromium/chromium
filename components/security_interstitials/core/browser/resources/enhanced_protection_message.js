// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SecurityInterstitialCommandId, sendCommand} from 'chrome://interstitials/common/resources/interstitial_common.js';
import {mobileNav} from 'chrome://interstitials/common/resources/interstitial_mobile_nav.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

// Other constants defined in security_interstitial_page.h.
const SB_DISPLAY_ENHANCED_PROTECTION_MESSAGE =
    'displayEnhancedProtectionMessage';

// This sets up the enhanced protection message.
export function setupEnhancedProtectionMessage() {
  const interstitialType = loadTimeData.getString('type');
  if (interstitialType !== 'SAFEBROWSING' && interstitialType !== 'SSL' &&
      interstitialType !== 'CAPTIVE_PORTAL') {
    return;
  }

  if (!loadTimeData.getBoolean(SB_DISPLAY_ENHANCED_PROTECTION_MESSAGE)) {
    return;
  }

  const enhancedProtectionLink =
      document.querySelector('#enhanced-protection-link');
  const enhancedProtectionMessage =
      document.querySelector('#enhanced-protection-message');
  if (enhancedProtectionLink) {
    if (mobileNav) {
      // To make sure the touch area of the link is larger than the
      // minimum touch area for accessibility, make the whole block tappable.
      enhancedProtectionMessage.addEventListener('click', function() {
        sendCommand(SecurityInterstitialCommandId
                        .CMD_OPEN_ENHANCED_PROTECTION_SETTINGS);
        return false;
      });
    } else {
      enhancedProtectionLink.addEventListener('click', function() {
        sendCommand(SecurityInterstitialCommandId
                        .CMD_OPEN_ENHANCED_PROTECTION_SETTINGS);
        return false;
      });
    }
  }
  enhancedProtectionMessage.classList.remove('hidden');

  const billing =
      interstitialType === 'SAFEBROWSING' && loadTimeData.getBoolean('billing');

  let className = 'ssl-enhanced-protection-message';
  if (interstitialType === 'SAFEBROWSING' && !billing) {
    className = 'safe-browsing-enhanced-protection-message';
  }

  enhancedProtectionMessage.classList.add(className);
}

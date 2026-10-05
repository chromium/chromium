// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Should match enterprise_net::EnterpriseProxyErrorData::ErrorCategory.
const EnterpriseProxyErrorCategory = {
  AUTHENTICATION: '0',
  AUTHORIZATION: '1',
  OTHER: '2',
};

// <if expr="is_ios">
// Should match security_interstitials::SecurityInterstitialCommand.
const SecurityInterstitialCommandId = {
  CMD_DONT_PROCEED: 0,
  CMD_OPEN_LOGIN: 7,
};

/**
 * Sends a command to the native iOS security interstitial page.
 * @param {number} cmd The command to send.
 */
function sendCommand(cmd) {
  window.webkit?.messageHandlers?.['IOSInterstitialMessage']?.postMessage(
      {'command': cmd.toString()});
}
// </if>

function goBackButtonClicked() {
  // <if expr="is_ios">
  sendCommand(SecurityInterstitialCommandId.CMD_DONT_PROCEED);
  // </if>
  // <if expr="not is_ios">
  if (window.history.length > 1) {
    window.history.back();
  } else {
    window.close();
  }
  // </if>
}

function signinButtonClicked() {
  // <if expr="is_ios">
  sendCommand(SecurityInterstitialCommandId.CMD_OPEN_LOGIN);
  // </if>
  // <if expr="not is_ios">
  if (window.errorPageController) {
    window.errorPageController.portalSigninButtonClick();
  } else {
    window.location.reload();
  }
  // </if>
}

document.addEventListener('DOMContentLoaded', () => {
  // TODO(crbug.com/563039777): Set the Learn More link URL once available.
  const learnMoreLink = document.getElementById('learn-more-link');
  if (learnMoreLink) {
    learnMoreLink.addEventListener('click', (e) => {
      e.preventDefault();
    });
  }

  const category = String(window.loadTimeDataRaw?.['error_category'] ?? '');
  if (category === EnterpriseProxyErrorCategory.AUTHENTICATION) {
    document.body.classList.add('category-authentication');
  } else if (category === EnterpriseProxyErrorCategory.AUTHORIZATION) {
    document.body.classList.add('category-authorization');
  } else {
    document.body.classList.add('category-other');
  }

  const primaryButton = document.getElementById('primary-button');
  if (primaryButton) {
    primaryButton.addEventListener(
        'click',
        category === EnterpriseProxyErrorCategory.AUTHENTICATION ?
            signinButtonClicked :
            goBackButtonClicked);
  }
});

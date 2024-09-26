// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: The following chrome:// URLs are not actually fetched at runtime. They
// are handled in
// components/security_interstitials/core/browser/resources:bundle_js, which
// finds the correct files and inlines them.
import {HIDDEN_CLASS, preventDefaultOnPoundLinkClicks, SecurityInterstitialCommandId, sendCommand} from 'chrome://interstitials/common/resources/interstitial_common.js';
import {mobileNav} from 'chrome://interstitials/common/resources/interstitial_mobile_nav.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {setupEnhancedProtectionMessage} from './enhanced_protection_message.js';
import {setupExtendedReportingCheckbox} from './extended_reporting.js';
import {setupSSLDebuggingInfo} from './ssl.js';

// This is the shared code for the new (Chrome 37) security interstitials. It is
// used for both SSL interstitials and Safe Browsing interstitials.

let expandedDetails = false;
let keyPressState = 0;

// Only begin clickjacking delay tracking when the DOM contents have
// fully loaded.
let timePageLastFocused = null;

// The amount of delay (in ms) before the proceed button accepts
// a "click" event.
const PROCEED_CLICKJACKING_DELAY = 500;

/**
 * This checks whether the clickjacking delay has been passed
 * since page was first loaded or last focused.
 * @return {boolean} Whether the clickjacking delay has passed or not.
 */
function clickjackingDelayHasPassed() {
  return (
      timePageLastFocused != null &&
      (window.performance.now() - timePageLastFocused >=
       PROCEED_CLICKJACKING_DELAY));
}

/**
 * This allows errors to be skippped by typing a secret phrase into the page.
 * @param {string} e The key that was just pressed.
 */
function handleKeypress(e) {
  // HTTPS errors are serious and should not be ignored. For testing purposes,
  // other approaches are both safer and have fewer side-effects.
  // See https://goo.gl/ZcZixP for more details.
  const BYPASS_SEQUENCE = window.atob('dGhpc2lzdW5zYWZl');
  if (BYPASS_SEQUENCE.charCodeAt(keyPressState) === e.keyCode) {
    keyPressState++;
    if (keyPressState === BYPASS_SEQUENCE.length) {
      sendCommand(SecurityInterstitialCommandId.CMD_PROCEED);
      keyPressState = 0;
    }
  } else {
    keyPressState = 0;
  }
}

function setupEvents() {
  // `loadTimeDataRaw` is injected to the `window` scope from C++.
  loadTimeData.data = window.loadTimeDataRaw;

  const overridable = loadTimeData.getBoolean('overridable');
  const interstitialType = loadTimeData.getString('type');
  const ssl = interstitialType === 'SSL';
  const captivePortal = interstitialType === 'CAPTIVE_PORTAL';
  const badClock = ssl && loadTimeData.getBoolean('bad_clock');
  const lookalike = interstitialType === 'LOOKALIKE';
  const billing =
      interstitialType === 'SAFEBROWSING' && loadTimeData.getBoolean('billing');
  const blockedInterception = interstitialType === 'BLOCKED_INTERCEPTION';
  const insecureForm = interstitialType == 'INSECURE_FORM';
  const httpsOnly = interstitialType == 'HTTPS_ONLY';
  const enterpriseBlock = interstitialType === 'ENTERPRISE_BLOCK';
  const enterpriseWarn = interstitialType === 'ENTERPRISE_WARN';
  const managedProfileRequired =
      interstitialType === 'MANAGED_PROFILE_REQUIRED';
  const supervisedUserVerify = interstitialType === 'SUPERVISED_USER_VERIFY';
  const supervisedUserVerifySubframe =
      interstitialType === 'SUPERVISED_USER_VERIFY_SUBFRAME';
  const hidePrimaryButton = loadTimeData.getBoolean('hide_primary_button');
  const showRecurrentErrorParagraph =
      loadTimeData.getBoolean('show_recurrent_error_paragraph');
  const showBlockedSiteMessage =
      loadTimeData.valueExists('show_blocked_site_message') ?
      loadTimeData.getBoolean('show_blocked_site_message') :
      false;

  const body = document.querySelector('#body');
  if (ssl || blockedInterception) {
    body.classList.add(badClock ? 'bad-clock' : 'ssl');
    if (loadTimeData.valueExists('errorCode')) {
      const errorCode = document.querySelector('#error-code');
      errorCode.textContent = loadTimeData.getString('errorCode');
      errorCode.classList.remove(HIDDEN_CLASS);
    }
  } else if (captivePortal) {
    body.classList.add('captive-portal');
  } else if (billing) {
    body.classList.add('safe-browsing-billing');
  } else if (lookalike) {
    body.classList.add('lookalike-url');
  } else if (insecureForm) {
    body.classList.add('insecure-form');
  } else if (httpsOnly) {
    body.classList.add('https-only');
    if (loadTimeData.valueExists('august2024Refresh') &&
        loadTimeData.getBoolean('august2024Refresh')) {
      body.classList.add('https-only-august2024-refresh');
    }
  } else if (enterpriseBlock) {
    body.classList.add('enterprise-block');
  } else if (enterpriseWarn) {
    body.classList.add('enterprise-warn');
  } else if (managedProfileRequired) {
    body.classList.add('managed-profile-required');
  } else if (supervisedUserVerify) {
    body.classList.add('supervised-user-verify');
  } else if (supervisedUserVerifySubframe) {
    body.classList.add('supervised-user-verify-subframe');
  } else {
    body.classList.add('safe-browsing');
    // Override the default theme color.
    document.querySelector('meta[name=theme-color]')
        .setAttribute('content', 'rgb(217, 48, 37)');
  }

  document.querySelector('#icon').classList.add('icon');

  const primaryButton = document.querySelector('#primary-button');
  if (hidePrimaryButton) {
    primaryButton.classList.add(HIDDEN_CLASS);
  } else {
    primaryButton.addEventListener('click', function() {
      switch (interstitialType) {
        case 'CAPTIVE_PORTAL':
        case 'SUPERVISED_USER_VERIFY':
        case 'SUPERVISED_USER_VERIFY_SUBFRAME':
          sendCommand(SecurityInterstitialCommandId.CMD_OPEN_LOGIN);
          break;

        case 'SSL':
          if (badClock) {
            sendCommand(SecurityInterstitialCommandId.CMD_OPEN_DATE_SETTINGS);
          } else if (overridable) {
            sendCommand(SecurityInterstitialCommandId.CMD_DONT_PROCEED);
          } else {
            sendCommand(SecurityInterstitialCommandId.CMD_RELOAD);
          }
          break;

        case 'SAFEBROWSING':
        case 'ENTERPRISE_BLOCK':
        case 'ENTERPRISE_WARN':
        case 'MANAGED_PROFILE_REQUIRED':
        case 'ORIGIN_POLICY':
          sendCommand(SecurityInterstitialCommandId.CMD_DONT_PROCEED);
          break;
        case 'HTTPS_ONLY':
        case 'INSECURE_FORM':
        case 'LOOKALIKE':
          sendCommand(SecurityInterstitialCommandId.CMD_DONT_PROCEED);
          break;

        default:
          throw new Error('Invalid interstitial type');
      }
    });
  }

  if (lookalike || insecureForm || httpsOnly || enterpriseWarn) {
    const proceedButton = document.querySelector('#proceed-button');
    proceedButton.classList.remove(HIDDEN_CLASS);
    proceedButton.textContent = loadTimeData.getString('proceedButtonText');
    proceedButton.addEventListener('click', function(event) {
      if (clickjackingDelayHasPassed()) {
        sendCommand(SecurityInterstitialCommandId.CMD_PROCEED);
      }
    });
  }
  if (lookalike) {
    // Lookalike interstitials with a suggested URL have a link in the title:
    // "Did you mean <link>example.com</link>?". Handle those clicks. Lookalike
    // interstitails without a suggested URL don't have this link.
    const dontProceedLink = document.querySelector('#dont-proceed-link');
    if (dontProceedLink) {
      dontProceedLink.addEventListener('click', function(event) {
        sendCommand(SecurityInterstitialCommandId.CMD_DONT_PROCEED);
      });
    }
  }

  if (overridable) {
    const overrideElement =
        document.querySelector(billing ? '#proceed-button' : '#proceed-link');
    // Captive portal page isn't overridable.
    overrideElement.addEventListener('click', function(event) {
      if (!billing || clickjackingDelayHasPassed()) {
        sendCommand(SecurityInterstitialCommandId.CMD_PROCEED);
      }
    });

    if (ssl) {
      overrideElement.classList.add('small-link');
    } else if (billing) {
      overrideElement.classList.remove(HIDDEN_CLASS);
      overrideElement.textContent = loadTimeData.getString('proceedButtonText');
    }
  } else if (!ssl) {
    document.querySelector('#final-paragraph').classList.add(HIDDEN_CLASS);
  }


  if (!ssl || !showRecurrentErrorParagraph) {
    document.querySelector('#recurrent-error-message')
        .classList.add(HIDDEN_CLASS);
  } else {
    body.classList.add('showing-recurrent-error-message');
  }

  if (showBlockedSiteMessage) {
    document.querySelector('#blocked-site-message')
        .classList.remove(HIDDEN_CLASS);
    body.classList.add('showing-blocked-site-message');
    document.getElementById('blocked-site-message-header').textContent =
        loadTimeData.getString('blockedSiteMessageHeader');
    document.getElementById('blocked-site-message-reason').textContent =
        loadTimeData.getString('blockedSiteMessageReason');
  }

  const diagnosticLink = document.querySelector('#diagnostic-link');
  if (diagnosticLink) {
    diagnosticLink.addEventListener('click', function(event) {
      sendCommand(SecurityInterstitialCommandId.CMD_OPEN_DIAGNOSTIC);
    });
  }

  const learnMoreLink = document.querySelector('#learn-more-link');
  if (learnMoreLink) {
    learnMoreLink.addEventListener('click', function(event) {
      sendCommand(SecurityInterstitialCommandId.CMD_OPEN_HELP_CENTER);
    });
  }

  const detailsButton = document.querySelector('#details-button');
  if (captivePortal || billing || lookalike || insecureForm || httpsOnly ||
      enterpriseWarn || enterpriseBlock || supervisedUserVerify ||
      managedProfileRequired || supervisedUserVerifySubframe) {
    // Captive portal, billing, lookalike pages, insecure form, enterprise warn,
    // enterprise block, and HTTPS only mode interstitials don't
    // have details buttons.
    detailsButton.classList.add('hidden');
  } else {
    detailsButton.setAttribute(
        'aria-expanded',
        !document.querySelector('#details').classList.contains(HIDDEN_CLASS));
    detailsButton.addEventListener('click', function(event) {
      const hiddenDetails =
          document.querySelector('#details').classList.toggle(HIDDEN_CLASS);
      detailsButton.setAttribute('aria-expanded', !hiddenDetails);

      const mainContent = document.querySelector('#main-content');
      if (mobileNav) {
        // Details appear over the main content on small screens.
        mainContent.classList.toggle(HIDDEN_CLASS, !hiddenDetails);
      } else {
        mainContent.classList.remove(HIDDEN_CLASS);
      }

      detailsButton.innerText = hiddenDetails ?
          loadTimeData.getString('openDetails') :
          loadTimeData.getString('closeDetails');
      if (!expandedDetails) {
        // Record a histogram entry only the first time that details is opened.
        sendCommand(SecurityInterstitialCommandId.CMD_SHOW_MORE_SECTION);
        expandedDetails = true;
      }
    });
  }

  const reportErrorLink = document.querySelector('#report-error-link');
  if (reportErrorLink) {
    reportErrorLink.addEventListener('click', function(event) {
      sendCommand(SecurityInterstitialCommandId.CMD_REPORT_PHISHING_ERROR);
    });
  }

  if (lookalike) {
    console.warn(loadTimeData.getString('lookalikeConsoleMessage'));
  }

  if (document.getElementById('icon')) {
    document.getElementById('icon').classList.add('new-icon');
  }

  preventDefaultOnPoundLinkClicks();
  setupExtendedReportingCheckbox();
  setupEnhancedProtectionMessage();
  setupSSLDebuggingInfo();
  document.addEventListener('keypress', handleKeypress);

  // Begin tracking for the clickjacking delay.
  timePageLastFocused = window.performance.now();
  window.addEventListener(
      'focus', () => timePageLastFocused = window.performance.now());
}

document.addEventListener('DOMContentLoaded', setupEvents);

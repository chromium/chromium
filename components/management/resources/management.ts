// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {getRequiredElement} from '//resources/js/util.js';

/**
 * Populates the Connectors section by conditionally showing
 * specific event sections based on flags.
 */
function populateConnectorsSection() {
  // Browser/profile reporting section. If both are active, only show the
  // browser reporting section.
  const browserReportingEnabled =
      loadTimeData.getBoolean('browserReportingEnabled');
  const profileReportingEnabled =
      loadTimeData.getBoolean('profileReportingEnabled');
  const deviceSignalsDisclosureEnabled =
      loadTimeData.getBoolean('deviceSignalsDisclosureEnabled');
  if (browserReportingEnabled) {
    getRequiredElement('browser-reporting-info').classList.remove('hidden');
  } else if (profileReportingEnabled) {
    getRequiredElement('profile-reporting-info').classList.remove('hidden');
  } else if (deviceSignalsDisclosureEnabled) {
    getRequiredElement('browser-reporting-info').classList.remove('hidden');
    for (const item of document.querySelectorAll<HTMLElement>(
             '.browser-reporting-item')) {
      item.classList.add('hidden');
    }
  }

  if (deviceSignalsDisclosureEnabled) {
    getRequiredElement('browser-signals-disclosure').classList.remove('hidden');
    getRequiredElement('profile-signals-disclosure').classList.remove('hidden');
  }

  const secureGatewayEnabled =
      loadTimeData.getBoolean('secureGatewayDisclosureEnabled');
  const pageVisitEnabled = loadTimeData.getBoolean('pageVisitEventEnabled');
  const securityEventEnabled = loadTimeData.getBoolean('securityEventEnabled');
  const fileDownloadEnabled =
      loadTimeData.getBoolean('fileDownloadEventEnabled');
  const textEnterEnabled = loadTimeData.getBoolean('textEnterEventEnabled');
  const hasConnectorEvents = pageVisitEnabled || securityEventEnabled ||
      fileDownloadEnabled || textEnterEnabled;
  const connectorsSectionVisible = secureGatewayEnabled || hasConnectorEvents;

  // Check if there are connectors enabled.
  if (connectorsSectionVisible) {
    getRequiredElement('connectors-info').classList.remove('hidden');

    if (secureGatewayEnabled) {
      getRequiredElement('secure-gateway-section').classList.remove('hidden');
    }

    if (hasConnectorEvents) {
      const threatProtectionMoreButton =
          getRequiredElement('threat-protection-more');
      threatProtectionMoreButton.classList.remove('hidden');
      threatProtectionMoreButton.addEventListener('click', () => {
        const expanded =
            threatProtectionMoreButton.getAttribute('aria-expanded') === 'true';
        threatProtectionMoreButton.setAttribute(
            'aria-expanded', String(!expanded));

        if (securityEventEnabled) {
          getRequiredElement('security-event-section')
              .classList.toggle('hidden', expanded);
        }

        if (pageVisitEnabled) {
          getRequiredElement('page-visit-event-section')
              .classList.toggle('hidden', expanded);
        }

        if (fileDownloadEnabled) {
          getRequiredElement('file-download-event-section')
              .classList.toggle('hidden', expanded);
        }

        if (textEnterEnabled) {
          getRequiredElement('text-enter-event-section')
              .classList.toggle('hidden', expanded);
        }
      });
    }
  }
}

// Listener ensures the DOM is fully loaded before manipulating elements
document.addEventListener('DOMContentLoaded', () => {
  if (loadTimeData.getBoolean('isManaged')) {
    getRequiredElement('managed-info').classList.remove('hidden');
    populateConnectorsSection();
  } else {
    getRequiredElement('unmanaged-info').classList.remove('hidden');
  }
});

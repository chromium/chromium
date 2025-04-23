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
  const pageVisitEnabled = loadTimeData.getBoolean('pageVisitEventEnabled');
  const securityEventEnabled = loadTimeData.getBoolean('securityEventEnabled');

  const connectorsSectionVisible = pageVisitEnabled || securityEventEnabled;

  // Check if there are connectors enabled.
  if (connectorsSectionVisible) {
    getRequiredElement('connectors-info').classList.remove('hidden');

    if (securityEventEnabled) {
      getRequiredElement('security-event-section').classList.remove('hidden');
    }

    if (pageVisitEnabled) {
      getRequiredElement('page-visit-event-section').classList.remove('hidden');
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

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getTemplate} from './status_box.html.js';

export class StatusBoxElement extends CustomElement {
  static get template() {
    return getTemplate();
  }

  /**
   * Sets the text of a particular named label element in the status box
   * and updates the visibility if needed.
   * @param {string} labelName The name of the label element that is being
   *     updated.
   * @param {string} labelValue The new text content for the label.
   * @param {boolean=} needsToBeShown True if we want to show the label
   *     False otherwise.
   */
  setLabelAndShow_(labelName, labelValue, needsToBeShown = true) {
    const labelElement = this.shadowRoot.querySelector(labelName);
    labelElement.textContent = labelValue ? ' ' + labelValue : '';
    if (needsToBeShown) {
      labelElement.parentElement.hidden = false;
    }
  }

  /**
   * Populate the box with the given cloud policy status.
   * @param {string} scope The policy scope, either "device", "machine",
   *     "user", or "updater".
   * @param {Object} status Dictionary with information about the status.
   */
  initialize(scope, status) {
    const notSpecifiedString = loadTimeData.getString('notSpecified');

    // Set appropriate box legend based on status key
    this.shadowRoot.querySelector('.legend').textContent =
        loadTimeData.getString(status.policyDescriptionKey);

    if (scope === 'device') {
      // Populate the device naming information.
      // Populate the asset identifier.
      this.setLabelAndShow_('.asset-id', status.assetId || notSpecifiedString);

      // Populate the device location.
      this.setLabelAndShow_('.location', status.location || notSpecifiedString);

      // Populate the directory API ID.
      this.setLabelAndShow_(
          '.directory-api-id', status.directoryApiId || notSpecifiedString);
      this.setLabelAndShow_('.client-id', status.clientId);
      // For off-hours policy, indicate if it's active or not.
      if (status.isOffHoursActive != null) {
        this.setLabelAndShow_(
            '.is-offhours-active',
            loadTimeData.getString(
                status.isOffHoursActive ? 'offHoursActive' :
                                          'offHoursNotActive'));
      }
    } else if (scope === 'machine') {
      this.setLabelAndShow_('.machine-enrollment-device-id', status.deviceId);
      this.setLabelAndShow_(
          '.machine-enrollment-token', status.enrollmentToken);
      if (status.machine) {
        this.setLabelAndShow_('.machine-enrollment-name', status.machine);
      }
      this.setLabelAndShow_('.machine-enrollment-domain', status.domain);
    } else if (scope === 'updater') {
      if (status.version) {
        this.setLabelAndShow_('.version', status.version);
      }
      if (status.domain) {
        this.setLabelAndShow_('.machine-enrollment-domain', status.domain);
      }
    } else {
      // Populate the topmost item with the username.
      this.setLabelAndShow_('.username', status.username);
      // Populate the user gaia id.
      this.setLabelAndShow_('.gaia-id', status.gaiaId || notSpecifiedString);
      this.setLabelAndShow_('.client-id', status.clientId);
      this.setLabelAndShow_('.profile-id', status.profileId);

      if (status.isAffiliated != null) {
        this.setLabelAndShow_(
            '.is-affiliated',
            loadTimeData.getString(
                status.isAffiliated ? 'isAffiliatedYes' : 'isAffiliatedNo'));
      }
    }

    if (status.enterpriseDomainManager) {
      this.setLabelAndShow_('.managed-by', status.enterpriseDomainManager);
    }

    if (status.timeSinceLastFetchAttempt) {
      this.setLabelAndShow_(
          '.time-since-last-fetch-attempt', status.timeSinceLastFetchAttempt);
    }

    if (status.timeSinceLastRefresh) {
      this.setLabelAndShow_(
          '.time-since-last-refresh', status.timeSinceLastRefresh);
    }

    if (scope !== 'updater') {
      if (status.refreshInterval) {
        this.setLabelAndShow_('.refresh-interval', status.refreshInterval);
      }
      this.setLabelAndShow_('.status', status.status);
      this.setLabelAndShow_(
          '.policy-push',
          loadTimeData.getString(
              status.policiesPushAvailable ? 'policiesPushOn' :
                                             'policiesPushOff'));
    }

    if (status.lastCloudReportSentTimestamp) {
      this.setLabelAndShow_(
          '.last-cloud-report-sent-timestamp',
          status.lastCloudReportSentTimestamp + ' (' +
              status.timeSinceLastCloudReportSent + ')');
    }

    if (status.error) {
      this.setLabelAndShow_(
          '.error', loadTimeData.getString('statusErrorManagedNoPolicy'));
    }
  }
}

customElements.define('status-box', StatusBoxElement);

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';

import {getTemplate} from './status_box.html.js';

export interface Status {
  policyDescriptionKey: string;
  flexOrgWarning: any;
  assetId?: string;
  location?: string;
  directoryApiId?: string;
  clientId: string;
  isOffHoursActive?: boolean;
  deviceId: string;
  enrollmentToken: string;
  domain: string;
  machine?: string;
  version?: string;
  username: string;
  gaiaId: string;
  profileId: string;
  status: string;
  refreshInterval: string;
  timeSinceLastRefresh: string;
  timeSinceLastFetchAttempt: string;
  enterpriseDomainManager: string;
  isAffiliated: boolean;
  lastCloudReportSentTimestamp: string;
  timeSinceLastCloudReportSent: string;
  policiesPushAvailable: boolean;
  error: boolean;
}

export class StatusBoxElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  /**
   * Sets the text of a particular named label element in the status box
   * and updates the visibility if needed.
   */
  private setLabelAndShow(
      labelName: string, labelValue: string, needsToBeShown: boolean = true) {
    const labelElement = this.shadowRoot!.querySelector(labelName);
    labelElement!.textContent = labelValue ? ' ' + labelValue : '';
    if (needsToBeShown) {
      labelElement!.parentElement!.hidden = false;
    }
  }

  /**
   * Sets the text of a particular named label element in the status box
   * and updates the visibility if needed.
   */
  private setLabelInnerHtmlAndShow(
      labelName: string, labelValue: string, needsToBeShown: boolean = true) {
    const labelElement = this.shadowRoot!.querySelector(labelName);
    labelElement!.innerHTML = sanitizeInnerHtml(` ${labelValue}`);
    if (needsToBeShown) {
      labelElement!.parentElement!.hidden = false;
    }
  }

  /**
   * Populate the box with the given cloud policy status. The policy scope,
   * either "device", "machine", "user", or "updater".
   */
  initialize(scope: string, status: Status) {
    const notSpecifiedString = loadTimeData.getString('notSpecified');

    // Set appropriate box heading based on status key.
    this.shadowRoot!.querySelector('.status-box-heading')!.textContent =
        loadTimeData.getString(status.policyDescriptionKey);
    if (status.flexOrgWarning) {
      this.setLabelInnerHtmlAndShow(
          '.warning', loadTimeData.getString('statusFlexOrgNoPolicy'), true);
      return;
    }
    if (scope === 'device') {
      // Populate the device naming information.
      // Populate the asset identifier.
      this.setLabelAndShow('.asset-id', status.assetId || notSpecifiedString);

      // Populate the device location.
      this.setLabelAndShow('.location', status.location || notSpecifiedString);

      // Populate the directory API ID.
      this.setLabelAndShow(
          '.directory-api-id', status.directoryApiId || notSpecifiedString);
      this.setLabelAndShow('.client-id', status.clientId);
      // For off-hours policy, indicate if it's active or not.
      if (status.isOffHoursActive != null) {
        this.setLabelAndShow(
            '.is-offhours-active',
            loadTimeData.getString(
                status.isOffHoursActive ? 'offHoursActive' :
                                          'offHoursNotActive'));
      }
    } else if (scope === 'machine') {
      this.setLabelAndShow('.machine-enrollment-device-id', status.deviceId);
      this.setLabelAndShow('.machine-enrollment-token', status.enrollmentToken);
      if (status.machine) {
        this.setLabelAndShow('.machine-enrollment-name', status.machine);
      }
      this.setLabelAndShow('.machine-enrollment-domain', status.domain);
    } else if (scope === 'updater') {
      if (status.version) {
        this.setLabelAndShow('.version', status.version);
      }
      if (status.domain) {
        this.setLabelAndShow('.machine-enrollment-domain', status.domain);
      }
    } else if (status.enrollmentToken) {
      this.setLabelAndShow('.machine-enrollment-domain', status.domain);
      this.setLabelAndShow('.machine-enrollment-token', status.enrollmentToken);
      this.setLabelAndShow('.client-id', status.clientId);
      this.setLabelAndShow('.profile-id', status.profileId);
    } else {
      // Populate the topmost item with the username.
      this.setLabelAndShow('.username', status.username);
      // Populate the user gaia id.
      this.setLabelAndShow('.gaia-id', status.gaiaId || notSpecifiedString);
      this.setLabelAndShow('.client-id', status.clientId);
      this.setLabelAndShow('.profile-id', status.profileId);

      if (status.isAffiliated != null) {
        this.setLabelAndShow(
            '.is-affiliated',
            loadTimeData.getString(
                status.isAffiliated ? 'isAffiliatedYes' : 'isAffiliatedNo'));
      }
    }

    if (status.enterpriseDomainManager) {
      this.setLabelAndShow('.managed-by', status.enterpriseDomainManager);
    }

    if (status.timeSinceLastFetchAttempt) {
      this.setLabelAndShow(
          '.time-since-last-fetch-attempt', status.timeSinceLastFetchAttempt);
    }

    if (status.timeSinceLastRefresh) {
      this.setLabelAndShow(
          '.time-since-last-refresh', status.timeSinceLastRefresh);
    }

    if (scope !== 'updater') {
      if (status.refreshInterval) {
        this.setLabelAndShow('.refresh-interval', status.refreshInterval);
      }
      this.setLabelAndShow('.status', status.status);
      this.setLabelAndShow(
          '.policy-push',
          loadTimeData.getString(
              status.policiesPushAvailable ? 'policiesPushOn' :
                                             'policiesPushOff'));
    }

    if (status.lastCloudReportSentTimestamp) {
      this.setLabelAndShow(
          '.last-cloud-report-sent-timestamp',
          status.lastCloudReportSentTimestamp + ' (' +
              status.timeSinceLastCloudReportSent + ')');
    }

    if (status.error) {
      this.setLabelAndShow(
          '.error', loadTimeData.getString('statusErrorManagedNoPolicy'));
    }
  }
}
declare global {
  interface HTMLElementTagNameMap {
    'status-box': StatusBoxElement;
  }
}
customElements.define('status-box', StatusBoxElement);

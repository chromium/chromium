// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AzureAdJoinType} from './policy.mojom-webui.js';
import type {Status} from './policy.mojom-webui.js';
import {getCss} from './status_box.css.js';
import {getHtml} from './status_box.html.js';

export interface StatusField {
  label: string;
  value: string;
  className: string;
  show: boolean;
}

export class StatusBoxElement extends CrLitElement {
  static get is() {
    return 'status-box';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      scope: {type: String},
      status: {type: Object},
    };
  }

  accessor scope: string = '';
  accessor status: Status|null = null;



  protected getGeneralFields(): StatusField[] {
    if (!this.status || this.status.flexOrgWarning) {
      return [];
    }

    const hasEnrollmentToken = !!this.status.enrollmentToken;

    return [
      {
        label: loadTimeData.getString('labelVersion'),
        value: this.status.version || '',
        className: 'version',
        show: this.scope === 'updater' && !!this.status.version,
      },
      {
        label: loadTimeData.getString('labelMachineEnrollmentMachineName'),
        value: this.status.machine || '',
        className: 'machine-enrollment-name',
        show: this.scope === 'machine' && !!this.status.machine,
      },
      {
        label: loadTimeData.getString('labelMachineEnrollmentDomain'),
        value: this.status.domain || '',
        className: 'machine-enrollment-domain',
        show: this.scope === 'machine' ||
            (this.scope === 'updater' && !!this.status.domain) ||
            (this.scope !== 'device' && hasEnrollmentToken),
      },
      {
        label: loadTimeData.getString('labelMachineEnrollmentToken'),
        value: this.status.enrollmentToken || '',
        className: 'machine-enrollment-token',
        show: this.scope === 'machine' ||
            (this.scope !== 'device' && this.scope !== 'updater' &&
             hasEnrollmentToken),
      },
      {
        label: loadTimeData.getString('labelMachineEntrollmentDeviceId'),
        value: this.status.deviceId || '',
        className: 'machine-enrollment-device-id',
        show: this.scope === 'machine',
      },
      // <if expr="is_win">
      {
        label: this.getAzureAdJoinStatusLabel(),
        value: this.getAzureAdJoinStatusString(),
        className: 'azure-ad-join-status',
        show: this.scope === 'machine' &&
            this.status.azureAdJoinType !== null &&
            this.status.azureAdJoinType !== undefined,
      },
      // </if>
      {
        label: loadTimeData.getString('labelUsername'),
        value: this.status.username ?? this.getNotSpecified(),
        className: 'username',
        show: this.scope !== 'device' && this.scope !== 'machine' &&
            this.scope !== 'updater' && !hasEnrollmentToken,
      },
      {
        label: loadTimeData.getString('labelManagedBy'),
        value: this.status.enterpriseDomainManager || '',
        className: 'managed-by',
        show: !!this.status.enterpriseDomainManager,
      },
      {
        label: loadTimeData.getString('labelGaiaId'),
        value: this.status.gaiaId || this.getNotSpecified(),
        className: 'gaia-id',
        show: this.scope !== 'device' && this.scope !== 'machine' &&
            this.scope !== 'updater' && !hasEnrollmentToken,
      },
      {
        label: loadTimeData.getString('labelClientId'),
        value: this.status.clientId || '',
        className: 'client-id',
        show: this.scope === 'device' ||
            (this.scope !== 'machine' && this.scope !== 'updater'),
      },
      {
        label: loadTimeData.getString('labelProfileId'),
        value: this.status.profileId ?? this.getNotSpecified(),
        className: 'profile-id',
        show: this.scope !== 'device' && this.scope !== 'machine' &&
            this.scope !== 'updater',
      },
      {
        label: loadTimeData.getString('labelAssetId'),
        value: this.status.assetId || this.getNotSpecified(),
        className: 'asset-id',
        show: this.scope === 'device',
      },
      {
        label: loadTimeData.getString('labelLocation'),
        value: this.status.location || this.getNotSpecified(),
        className: 'location',
        show: this.scope === 'device',
      },
      {
        label: loadTimeData.getString('labelDirectoryApiId'),
        value: this.status.directoryApiId || this.getNotSpecified(),
        className: 'directory-api-id',
        show: this.scope === 'device',
      },
      {
        label: loadTimeData.getString('labelRefreshInterval'),
        value: this.status.refreshInterval || '',
        className: 'refresh-interval',
        show: this.scope !== 'updater' && !!this.status.refreshInterval,
      },
      {
        label: loadTimeData.getString('labelPoliciesPush'),
        value: this.getPolicyPushString(),
        className: 'policy-push',
        show: this.scope !== 'updater' &&
            this.status.policiesPushAvailable !== null &&
            this.status.policiesPushAvailable !== undefined,
      },
      {
        label: loadTimeData.getString('labelIsAffiliated'),
        value: this.getIsAffiliatedString(),
        className: 'is-affiliated',
        show: this.scope !== 'device' && this.scope !== 'machine' &&
            this.scope !== 'updater' && !hasEnrollmentToken &&
            this.status.isAffiliated !== null &&
            this.status.isAffiliated !== undefined,
      },
      {
        label: loadTimeData.getString('labelIsOffHoursActive'),
        value: this.getIsOffHoursActiveString(),
        className: 'is-offhours-active',
        show: this.scope === 'device' &&
            this.status.isOffHoursActive !== null &&
            this.status.isOffHoursActive !== undefined,
      },
      {
        label: loadTimeData.getString('labelLastCloudReportSentTimestamp'),
        value: this.getLastCloudReportSentTimestampString(),
        className: 'last-cloud-report-sent-timestamp',
        show: !!this.status.lastCloudReportSentTimestamp,
      },
    ];
  }

  protected getPolicyFetchFields(): StatusField[] {
    if (!this.status || this.status.flexOrgWarning) {
      return [];
    }

    return [
      {
        label: loadTimeData.getString('labelTimeSinceLastFetchAttempt'),
        value: this.status.timeSinceLastFetchAttempt || '',
        className: 'time-since-last-fetch-attempt',
        show: !!this.status.timeSinceLastFetchAttempt,
      },
      {
        label: loadTimeData.getString('labelTimeSinceLastRefresh'),
        value: this.status.timeSinceLastRefresh || '',
        className: 'time-since-last-refresh',
        show: !!this.status.timeSinceLastRefresh,
      },
      {
        label: loadTimeData.getString('labelStatus'),
        value: this.status.status || '',
        className: 'status',
        show: this.scope !== 'updater' && !!this.status.status,
      },
      {
        label: loadTimeData.getString('labelError') + ':',
        value: this.getPolicyFetchErrorString(),
        className: 'error',
        show: this.status.error,
      },
    ];
  }

  protected getExtensionInstallFields(): StatusField[] {
    if (!this.status || this.status.flexOrgWarning) {
      return [];
    }

    return [
      {
        label: loadTimeData.getString('labelTimeSinceLastFetchAttempt'),
        value: this.status.extensionInstallTimeSinceLastFetchAttempt || '',
        className: 'extension-install-time-since-last-fetch-attempt',
        show: !!this.status.extensionInstallTimeSinceLastFetchAttempt,
      },
      {
        label: loadTimeData.getString('labelTimeSinceLastRefresh'),
        value: this.status.extensionInstallTimeSinceLastRefresh || '',
        className: 'extension-install-time-since-last-refresh',
        show: !!this.status.extensionInstallTimeSinceLastRefresh,
      },
      {
        label: loadTimeData.getString('labelStatus'),
        value: this.status.extensionInstallStatus || '',
        className: 'extension-install-status',
        show: !!this.status.extensionInstallStatus,
      },
      {
        label: loadTimeData.getString('labelError') + ':',
        value: this.getExtensionInstallErrorString(),
        className: 'extension-install-error',
        show: this.status.extensionInstallError,
      },
    ];
  }

  protected getHeading(): string {
    return this.status ?
        loadTimeData.getString(this.status.policyDescriptionKey) :
        '';
  }

  protected showFlexOrgWarning(): boolean {
    return !!this.status?.flexOrgWarning;
  }

// <if expr="not is_ios">
  protected getWarning(): TrustedHTML {
    const warning = this.status?.flexOrgWarning ?
        ` ${loadTimeData.getString('statusFlexOrgNoPolicy')}` :
        '';
    return sanitizeInnerHtml(warning);
  }
// </if>

// <if expr="is_ios">
  protected getWarning(): string {
    const warning = this.status?.flexOrgWarning ?
        ` ${loadTimeData.getString('statusFlexOrgNoPolicy')}` :
        '';
    return sanitizeInnerHtml(warning);
  }
// </if>

  private getNotSpecified(): string {
    return loadTimeData.getString('notSpecified');
  }

  private getAzureAdJoinStatusLabel(): string {
    return loadTimeData.valueExists('labelAzureAdJoinStatus') ?
        loadTimeData.getString('labelAzureAdJoinStatus') :
        '';
  }

  private getAzureAdJoinStatusString(): string {
    switch (this.status?.azureAdJoinType) {
      case AzureAdJoinType.kNotJoined:
        return loadTimeData.getString('azureAdNotJoined');
      case AzureAdJoinType.kWorkplace:
        return loadTimeData.getString('azureAdWorkplaceJoined');
      case AzureAdJoinType.kDevice:
        return loadTimeData.getString('azureAdDeviceJoined');
      case AzureAdJoinType.kUnknown:
        return loadTimeData.getString('azureAdJoinedUnknown');
      default:
        return '';
    }
  }

  protected showPolicyFetchSection(): boolean {
    if (!this.status || this.status.flexOrgWarning) {
      return false;
    }
    return !!(
        this.status.timeSinceLastFetchAttempt ||
        this.status.timeSinceLastRefresh || this.status.error);
  }

  protected showExtensionInstallSection(): boolean {
    if (!this.status || this.status.flexOrgWarning) {
      return false;
    }
    return !!(
        this.status.extensionInstallTimeSinceLastFetchAttempt ||
        this.status.extensionInstallTimeSinceLastRefresh ||
        this.status.extensionInstallError);
  }

  private getIsAffiliatedString(): string {
    if (!this.status || this.status.isAffiliated === undefined) {
      return '';
    }
    return loadTimeData.getString(
        this.status.isAffiliated ? 'isAffiliatedYes' : 'isAffiliatedNo');
  }

  private getIsOffHoursActiveString(): string {
    if (!this.status || this.status.isOffHoursActive === undefined) {
      return '';
    }
    return loadTimeData.getString(
        this.status.isOffHoursActive ? 'offHoursActive' : 'offHoursNotActive');
  }

  private getPolicyPushString(): string {
    if (!this.status) {
      return '';
    }
    return loadTimeData.getString(
        this.status.policiesPushAvailable ? 'policiesPushOn' :
                                            'policiesPushOff');
  }

  private getLastCloudReportSentTimestampString(): string {
    if (!this.status || !this.status.lastCloudReportSentTimestamp) {
      return '';
    }
    return this.status.lastCloudReportSentTimestamp + ' (' +
        this.status.timeSinceLastCloudReportSent + ')';
  }

  private getPolicyFetchErrorString(): string {
    return this.status?.error ?
        loadTimeData.getString('statusErrorManagedNoPolicy') :
        '';
  }

  private getExtensionInstallErrorString(): string {
    return this.status?.extensionInstallError ?
        loadTimeData.getString('statusErrorManagedNoPolicy') :
        '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'status-box': StatusBoxElement;
  }
}

customElements.define(StatusBoxElement.is, StatusBoxElement);

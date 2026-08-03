// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/js/action_link.js';
// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';
// </if>

import './status_box.js';
import './policy_table.js';
// <if expr="not is_ios and not is_android">
import './promotion_banner_section_container.js';
// </if>
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {BrowserProxy} from './browser_proxy.js';
import {GetPoliciesReason} from './policy.mojom-webui.js';
import type {Status} from './policy.mojom-webui.js';
import {getCss} from './policy_app.css.js';
import {getHtml} from './policy_app.html.js';
import type {Policy} from './policy_row.js';
import type {PolicyTableModel} from './policy_table.js';



export interface PolicyNamesResponse {
  [id: string]: {name: string, policyNames: NonNullable<string[]>};
}

export interface PolicyValues {
  [id: string]: {
    name: string,
    policies: {[name: string]: Policy},
    precedenceOrder?: string[],
    isExtension?: boolean,
    forSigninScreen?: boolean,
  };
}

export interface PolicyValuesResponse {
  policyIds: string[];
  policyValues: PolicyValues;
}

export class PolicyAppElement extends CrLitElement {
  static get is() {
    return 'policy-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      filterPattern_: {type: String},
      showUnset_: {type: Boolean},
      status_: {type: Object},
      policyGroups_: {type: Array},
      shouldShowPromo_: {type: Boolean},
      shouldShowCommandLineFlagsWarning_: {type: Boolean},
      toastText_: {type: String},
      reloadButtonDisabled_: {type: Boolean},
      uploadReportButtonDisabled_: {type: Boolean},
      enableReportButton_: {type: Boolean},
      hideExportButton_: {type: Boolean},
      hideUploadReportButton_: {type: Boolean},
    };
  }

  protected accessor filterPattern_: string = '';
  protected accessor showUnset_: boolean = false;
  protected accessor status_: Record<string, Status> = {};
  protected accessor policyGroups_: PolicyTableModel[] = [];
  protected accessor shouldShowPromo_: boolean = false;
  protected accessor shouldShowCommandLineFlagsWarning_: boolean = false;
  protected accessor toastText_: string = '';
  protected accessor reloadButtonDisabled_: boolean = false;
  protected accessor uploadReportButtonDisabled_: boolean = false;
  protected accessor enableReportButton_: boolean = false;

  protected accessor hideExportButton_ = false;
  protected accessor hideUploadReportButton_ = false;


  private toastTimeoutId_: number|null = null;
  private policyPageMojoMigrationEnabled_ =
      loadTimeData.getBoolean('policyPageMojoMigrationEnabled');

  override async connectedCallback() {
    super.connectedCallback();

    if (this.policyPageMojoMigrationEnabled_) {
      const message = await BrowserProxy.getDebugString();
      console.info(message);
    }

    if (!loadTimeData.getString('acceptedPaths')
             .split('|')
             .includes(window.location.pathname)) {
      window.history.replaceState({}, '', '/');
    }

    FocusOutlineManager.forDocument(document);

    // <if expr="not is_ios and not is_android">
    this.shouldShowPromo_ = await BrowserProxy.checkPromotionEligibility();
    this.shouldShowCommandLineFlagsWarning_ =
        loadTimeData.valueExists('hasCustomCommandLineFlags') ?
        loadTimeData.getBoolean('hasCustomCommandLineFlags') :
        await BrowserProxy.checkCommandLineSwitches();
    // </if>

    this.hideExportButton_ = loadTimeData.valueExists('hideExportButton') &&
        loadTimeData.getBoolean('hideExportButton');
    this.hideUploadReportButton_ =
        loadTimeData.valueExists('hideUploadReportButton') &&
        loadTimeData.getBoolean('hideUploadReportButton');

    sendWithPromise<void>('listenPoliciesUpdates');
    addWebUiListener(
        'status-updated',
        (status: Record<string, Status>) => this.status_ = status);
    addWebUiListener(
        'policies-updated',
        (names: PolicyNamesResponse, values: PolicyValuesResponse) =>
            this.onPoliciesReceived_(names, values));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
  }

  // <if expr="not is_ios and not is_android">
  protected onPromoDismiss_() {
    BrowserProxy.setBannerDismissed();
    this.shouldShowPromo_ = false;
  }

  protected onPromoRedirect_() {
    BrowserProxy.recordBannerRedirected();
    window.open(
        'https://admin.google.com/ac/chrome/guides/' +
            '?ref=browser&utm_source=chrome_policy_cec',
        '_blank',
    );
  }
  // </if>

  private onPoliciesReceived_(
      policyNames: PolicyNamesResponse,
      policyValuesResponse: PolicyValuesResponse) {
    const policyValues: PolicyValues = policyValuesResponse.policyValues;
    const policyIds: string[] = policyValuesResponse.policyIds;

    this.policyGroups_ = policyIds.map((id: string) => {
      const knownPolicyNames =
          policyNames[id] ? policyNames[id].policyNames : [];
      const value = policyValues[id]!;
      const knownPolicyNamesSet = new Set(knownPolicyNames);
      const receivedPolicyNames =
          value.policies ? Object.keys(value.policies) : [];
      const allPolicyNames =
          Array.from(new Set([...knownPolicyNames, ...receivedPolicyNames]));
      const policies = allPolicyNames.map(
          name => Object.assign(
              {
                name,
                link: [
                  policyNames['chrome']?.policyNames,
                  policyNames['precedence']?.policyNames,
                ].includes(knownPolicyNames) &&
                        knownPolicyNamesSet.has(name) ?
                    `https://chromeenterprise.google/policies/?policy=${name}` :
                    undefined,
                isExtension: value.isExtension || false,
              },
              value?.policies[name]));

      return {
        name: value.forSigninScreen ?
            `${value.name} [${loadTimeData.getString('signinProfile')}]` :
            value.name,
        id: id,
        policies,
        ...(value.precedenceOrder && {precedenceOrder: value.precedenceOrder}),
      };
    });

    // <if expr="not is_chromeos">
    this.enableReportButton_ =
        [
          'CloudReportingEnabled',
          'CloudProfileReportingEnabled',
          'UserSecuritySignalsReporting',
        ].map(p => !!policyValues['chrome']?.policies[p]?.value)
            .reduce((accumulator, current) => accumulator ||= current, false);
    // </if>
    this.reloadPoliciesDone_();
  }



  protected onSearchChanged_(e: CustomEvent<string>) {
    this.filterPattern_ = e.detail.toLowerCase();
  }

  protected onReloadPoliciesClick_() {
    this.reloadButtonDisabled_ = true;
    this.showToast_(loadTimeData.getString('reloadingPolicies'));
    sendWithPromise<void>('reloadPolicies');
  }

  protected onMoreActionsClick_(e: Event) {
    this.shadowRoot.querySelector<CrActionMenuElement>('#actionMenu')!.showAt(
        e.target as HTMLElement);
  }

  protected async onExportPoliciesClick_() {
    this.closeActionMenu_();
    const policies = await BrowserProxy.getPolicies(GetPoliciesReason.kExport);
    this.downloadJson_(policies);
  }

  protected async onCopyPoliciesClick_() {
    this.closeActionMenu_();
    const policies = await BrowserProxy.getPolicies(GetPoliciesReason.kCopy);
    navigator.clipboard.writeText(policies);
    this.showToast_(loadTimeData.getString('copyPoliciesDone'));
  }

  protected onUploadReportClick_() {
    this.closeActionMenu_();
    this.uploadReportButtonDisabled_ = true;
    this.showToast_(loadTimeData.getString('reportUploading'));
    sendWithPromise<void>('uploadReport').then(() => {
      this.uploadReportButtonDisabled_ = false;
      this.showToast_(loadTimeData.getString('reportUploaded'));
    });
  }

  protected onViewLogsClick_() {
    this.closeActionMenu_();
    window.location.href = 'chrome://policy/logs';
  }

  protected onShowUnsetCheckedChanged_(e: CustomEvent<{value: boolean}>) {
    this.showUnset_ = e.detail.value;
  }

  protected hasStatus_(): boolean {
    return Object.values(this.status_)
        .some(boxStatus => !!boxStatus.policyDescriptionKey);
  }

  private closeActionMenu_() {
    this.shadowRoot.querySelector<CrActionMenuElement>('#actionMenu')!.close();
  }

  private showToast_(content: string) {
    this.toastText_ = content;
    if (this.toastTimeoutId_) {
      clearTimeout(this.toastTimeoutId_);
    }
    this.toastTimeoutId_ = setTimeout(() => {
      this.toastText_ = '';
      this.toastTimeoutId_ = null;
    }, 2000);
  }

  private downloadJson_(json: string) {
    const jsonObject = JSON.parse(json);
    const timestamp = new Date(Date.now()).toLocaleString(undefined, {
      dateStyle: 'short',
      timeStyle: 'long',
    });

    jsonObject.policyExportTime = timestamp;
    const blob = new Blob(
        [JSON.stringify(jsonObject, null, 3)], {type: 'application/json'});
    const blobUrl = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = blobUrl;
    link.download =
        `policies_${timestamp.replace(/ GMT[+-]\d+:\d+/g, '')}.json`;

    document.body.appendChild(link);
    link.dispatchEvent(new MouseEvent(
        'click', {bubbles: true, cancelable: true, view: window}));
    document.body.removeChild(link);
    this.showToast_(loadTimeData.getString('exportPoliciesDone'));
  }

  private reloadPoliciesDone_() {
    if (this.reloadButtonDisabled_) {
      this.reloadButtonDisabled_ = false;
      this.showToast_(loadTimeData.getString('reloadPoliciesDone'));
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'policy-app': PolicyAppElement;
  }
}

customElements.define(PolicyAppElement.is, PolicyAppElement);

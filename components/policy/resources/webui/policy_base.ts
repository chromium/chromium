// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import 'chrome://resources/js/action_link.js';
// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';
// </if>

import './status_box.js';
import './policy_table.js';

import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import type {Policy} from './policy_row.js';
import type {PolicyTableElement, PolicyTableModel} from './policy_table.js';
import type {Status, StatusBoxElement} from './status_box.js';

export interface PolicyNamesResponse {
  [id: string]: {name: string, policyNames: NonNullable<string[]>};
}

export interface PolicyValues {
  [id: string]: {
    name: string,
    policies: {[name: string]: Policy},
    precedenceOrder?: string[],
  };
}

export interface PolicyValuesResponse {
  policyIds: string[];
  policyValues: PolicyValues;
}

// A singleton object that handles communication between browser and WebUI.
export class Page {
  mainSection: Element;
  policyTables: {[id: string]: PolicyTableElement};

  constructor() {
    this.policyTables = {};
  }

  /**
   * Main initialization function. Called by the browser on page load.
   */
  initialize() {
    // The default path is loaded when one path is not supported, so simple
    // redirect to the home path
    if (!loadTimeData.getString('acceptedPaths')
             .split('|')
             .includes(window.location.pathname)) {
      window.history.replaceState({}, '', '/');
    }

    FocusOutlineManager.forDocument(document);

    this.mainSection = getRequiredElement('main-section');

    const policyElement = getRequiredElement('policy-ui');
    // Add or remove header shadow based on scroll position.
    policyElement.addEventListener('scroll', () => {
      document.getElementsByTagName('header')[0]!.classList.toggle(
          'header-shadow', policyElement.scrollTop > 0);
    });

    // Place the initial focus on the search input field.
    const filterElement =
        getRequiredElement('search-field-input') as HTMLInputElement;
    filterElement.focus();

    filterElement.addEventListener('search', () => {
      for (const policyTable in this.policyTables) {
        this.policyTables[policyTable]!.setFilterPattern(
            filterElement.value as string);
      }
    });

    const reloadPoliciesButton =
        getRequiredElement('reload-policies') as HTMLButtonElement;
    reloadPoliciesButton.onclick = () => {
      reloadPoliciesButton!.disabled = true;
      this.createToast(loadTimeData.getString('reloadingPolicies'));
      sendWithPromise('reloadPolicies');
    };

    const moreActionsButton =
        getRequiredElement('more-actions-button') as HTMLButtonElement;
    const moreActionsIcon = getRequiredElement('dropdown-icon') as HTMLElement;
    const moreActionsList =
        getRequiredElement('more-actions-list') as HTMLElement;
    moreActionsButton.onclick = () => {
      moreActionsList!.classList.toggle('more-actions-visibility');
    };

    // Close dropdown if user clicks anywhere on page.
    document.addEventListener('click', function(event) {
      if (moreActionsList && event.target !== moreActionsButton &&
          event.target !== moreActionsIcon) {
        moreActionsList.classList.add('more-actions-visibility');
      }
    });

    const exportButton = getRequiredElement('export-policies');
    const hideExportButton = loadTimeData.valueExists('hideExportButton') &&
        loadTimeData.getBoolean('hideExportButton');
    if (hideExportButton) {
      exportButton.style.display = 'none';
    } else {
      exportButton.onclick = () => {
        sendWithPromise('exportPoliciesJSON');
      };
    }

    // <if expr="not is_chromeos">
    // Hide report button by default, will be displayed once we have policy
    // value.
    const uploadReportButton =
        getRequiredElement('upload-report') as HTMLButtonElement;
    uploadReportButton.style.display = 'none';
    uploadReportButton.onclick = () => {
      uploadReportButton.disabled = true;
      this.createToast(loadTimeData.getString('reportUploading'));
      sendWithPromise('uploadReport').then(() => {
        uploadReportButton.disabled = false;
        this.createToast(loadTimeData.getString('reportUploaded'));
      });
    };
    // </if>

    getRequiredElement('copy-policies').onclick = () => {
      sendWithPromise('copyPoliciesJSON');
      this.createToast(loadTimeData.getString('copyPoliciesDone'));
    };

    getRequiredElement('show-unset').onchange = () => {
      for (const policyTable in this.policyTables) {
        this.policyTables[policyTable]?.filter();
      }
    };

    sendWithPromise('listenPoliciesUpdates');
    addWebUiListener(
        'status-updated', (status: Status) => this.setStatus(status));
    addWebUiListener(
        'policies-updated',
        (names: PolicyNamesResponse, values: PolicyValuesResponse) =>
            this.onPoliciesReceived_(names, values));
    addWebUiListener(
        'download-json', (json: string) => this.downloadJson(json));

    sendWithPromise('shouldShowPromotion').then((shouldShowPromo: boolean) => {
        const bannerSection = getRequiredElement('promotion-banner-section');
            if(shouldShowPromo){
              bannerSection.hidden = false;
            }
    });
  }

  private onPoliciesReceived_(
      policyNames: PolicyNamesResponse,
      policyValuesResponse: PolicyValuesResponse) {
    const policyValues: PolicyValues = policyValuesResponse.policyValues;
    const policyIds: string[] = policyValuesResponse.policyIds;

    const policyGroups: Array<NonNullable<PolicyTableModel>> =
        policyIds.map((id: string) => {
          const knownPolicyNames =
              policyNames[id] ? policyNames[id]!.policyNames : [];
          const value: any = policyValues[id];
          const knownPolicyNamesSet = new Set(knownPolicyNames);
          const receivedPolicyNames =
              value.policies ? Object.keys(value.policies) : [];
          const allPolicyNames = Array.from(
              new Set([...knownPolicyNames, ...receivedPolicyNames]));
          const policies = allPolicyNames.map(
              name => Object.assign(
                  {
                    name,
                    link: [
                      policyNames['chrome']?.policyNames,
                      policyNames['precedence']?.policyNames,
                    ].includes(knownPolicyNames) &&
                            knownPolicyNamesSet.has(name) ?
                        `https://chromeenterprise.google/policies/?policy=${
                            name}` :
                        undefined,
                  },
                  value?.policies[name]));

          return {
            name: value.forSigninScreen ?
                `${value.name} [${loadTimeData.getString('signinProfile')}]` :
                value.name,
            id: value.isExtension ? id : null,
            policies,
            ...(value.precedenceOrder &&
                {precedenceOrder: value.precedenceOrder}),
          };
        });

    policyGroups.forEach(group => this.createOrUpdatePolicyTable(group));

    // <if expr="not is_chromeos">
    this.updateReportButton(
      !!policyValues['chrome']?.policies['CloudReportingEnabled']?.value ||
      !!policyValues['chrome']?.policies['CloudProfileReportingEnabled']?.value,
    );
    // </if>
    this.reloadPoliciesDone();
  }

  /**
   * Creates a toast notification with 2 second timeout at bottom of the page.
   * The notification is also announced to screen readers.
   */
  createToast(content: string): void {
    const toast = document.createElement('div');
    toast.textContent = content;
    toast.classList.add('toast');
    toast.setAttribute('role', 'alert');
    const container = getRequiredElement('toast-container');
    container.appendChild(toast);

    setTimeout(() => {
      container.removeChild(toast);
    }, 2000);
  }

  // Triggers the download of the policies as a JSON file.
  downloadJson(json: string) {
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
    // Regex matches GMT timezone pattern, such as "GMT+5:30"
    link.download =
        `policies_${timestamp.replace(/ GMT[+-]\d+:\d+/g, '')}.json`;

    document.body.appendChild(link);

    link.dispatchEvent(new MouseEvent(
        'click', {bubbles: true, cancelable: true, view: window}));

    document.body.removeChild(link);
    this.createToast(loadTimeData.getString('exportPoliciesDone'));
  }

  createOrUpdatePolicyTable(dataModel: PolicyTableModel) {
    const id = `${dataModel.name}-${dataModel.id}`;
    if (!this.policyTables[id]) {
      this.policyTables[id] = document.createElement('policy-table');
      this.mainSection!.appendChild(this.policyTables[id]!);
      this.policyTables[id]!.addEventListeners();
    }
    this.policyTables[id]!.updateDataModel(dataModel);
  }

  /**
   * Update the status section of the page to show the current cloud policy
   * status.
   * Status is the dictionary containing the current policy status.
   */
  setStatus(status: {[key: string]: any}) {
    // Remove any existing status boxes.
    const container = getRequiredElement('status-box-container');
    while (container.firstChild) {
      container.removeChild(container.firstChild);
    }
    // Hide the status section.
    const section = getRequiredElement('status-section');
    section!.hidden = true;

    // Add a status box for each scope that has a cloud policy status.
    for (const scope in status) {
      const boxStatus: Status = status[scope];
      if (!boxStatus.policyDescriptionKey) {
        continue;
      }
      const box = document.createElement('status-box') as StatusBoxElement;
      box.initialize(scope, boxStatus);
      container.appendChild(box);
      // Show the status section.
      section!.hidden = false;
    }
  }

  /**
   * Re-enable the reload policies button when the previous request to reload
   * policies values has completed.
   */
  reloadPoliciesDone() {
    const reloadButton =
        getRequiredElement('reload-policies') as HTMLButtonElement;
    if (reloadButton!.disabled) {
      reloadButton!.disabled = false;
      this.createToast(loadTimeData.getString('reloadPoliciesDone'));
    }
  }

  // <if expr="not is_chromeos">
  /**
   * Show report button if it's `enabled` by the policy. Exclude CrOS as there
   * are multiple report on CrOS but the button doesn't support all of them so
   * far.
   */
  updateReportButton(enabled: boolean) {
    getRequiredElement('upload-report').style.display =
        enabled ? 'block' : 'none';
  }
  // </if>

  static getInstance() {
    return instance || (instance = new Page());
  }
}

// Make Page a singleton.
let instance: Page;

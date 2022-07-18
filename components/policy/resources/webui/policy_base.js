// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import 'chrome://resources/js/action_link.js';
// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';
// </if>

import './status_box.js';
import './policy_table.js';

import {addSingletonGetter, addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {FocusOutlineManager} from 'chrome://resources/js/cr/ui/focus_outline_manager.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {$} from 'chrome://resources/js/util.m.js';

/**
 * @typedef {{
 *    [id: string]: {
 *      name: string,
 *      policyNames: !Array<string>,
 * }}
 */
let PolicyNamesResponse;

/**
 * @typedef {!Array<{
 *  name: string,
 *  id: ?String,
 *  policies: {[name: string]: policy.Policy},
 *  precedenceOrder: ?Array<string>,
 * }>}
 */
let PolicyValuesResponse;

/**
 * A singleton object that handles communication between browser and WebUI.
 */
export class Page {
  constructor() {
    /** @type {?Element} */
    this.mainSection = null;

    /** @type {{[id: string]: PolicyTable}} */
    this.policyTables = {};
  }

  /**
   * Main initialization function. Called by the browser on page load.
   */
  initialize() {
    FocusOutlineManager.forDocument(document);

    this.mainSection = $('main-section');

    // Place the initial focus on the filter input field.
    $('filter').focus();

    $('filter').onsearch = () => {
      for (const policyTable in this.policyTables) {
        this.policyTables[policyTable].setFilterPattern($('filter').value);
      }
    };
    $('reload-policies').onclick = () => {
      $('reload-policies').disabled = true;
      $('screen-reader-message').textContent =
          loadTimeData.getString('loadingPolicies');
      chrome.send('reloadPolicies');
    };

    const exportButton = $('export-policies');
    const hideExportButton = loadTimeData.valueExists('hideExportButton') &&
        loadTimeData.getBoolean('hideExportButton');
    if (hideExportButton) {
      exportButton.style.display = 'none';
    } else {
      exportButton.onclick = () => {
        chrome.send('exportPoliciesJSON');
      };
    }

    $('copy-policies').onclick = () => {
      chrome.send('copyPoliciesJSON');
    };

    $('show-unset').onchange = () => {
      for (const policyTable in this.policyTables) {
        this.policyTables[policyTable].filter();
      }
    };

    chrome.send('listenPoliciesUpdates');
    addWebUIListener('status-updated', status => this.setStatus(status));
    addWebUIListener(
        'policies-updated',
        (names, values) => this.onPoliciesReceived_(names, values));
    addWebUIListener('download-json', json => this.downloadJson(json));
  }

  /**
   * @param {PolicyNamesResponse} policyNames
   * @param {PolicyValuesResponse} policyValues
   * @private
   */
  onPoliciesReceived_(policyNames, policyValues) {
    /** @type {Array<!PolicyTableModel>} */
    const policyGroups = policyValues.map(value => {
      const knownPolicyNames =
          policyNames[value.id] ? policyNames[value.id].policyNames : [];
      const knownPolicyNamesSet = new Set(knownPolicyNames);
      const receivedPolicyNames = Object.keys(value.policies);
      const allPolicyNames =
          Array.from(new Set([...knownPolicyNames, ...receivedPolicyNames]));
      const policies = allPolicyNames.map(
          name => Object.assign(
              {
                name,
                link: [
                  policyNames.chrome.policyNames,
                  policyNames.precedence?.policyNames,
                ].includes(knownPolicyNames) &&
                        knownPolicyNamesSet.has(name) ?
                    `https://chromeenterprise.google/policies/?policy=${name}` :
                    undefined,
              },
              value.policies[name]));

      return {
        name: value.forSigninScreen ?
            `${value.name} [${loadTimeData.getString('signinProfile')}]` :
            value.name,
        id: value.isExtension ? value.id : null,
        policies,
        ...(value.precedenceOrder && {precedenceOrder: value.precedenceOrder}),
      };
    });

    policyGroups.forEach(group => this.createOrUpdatePolicyTable(group));

    this.reloadPoliciesDone();
  }

  /**
   * Triggers the download of the policies as a JSON file.
   * @param {String} json The policies as a JSON string.
   */
  downloadJson(json) {
    const blob = new Blob([json], {type: 'application/json'});
    const blobUrl = URL.createObjectURL(blob);

    const link = document.createElement('a');
    link.href = blobUrl;
    link.download = 'policies.json';

    document.body.appendChild(link);

    link.dispatchEvent(new MouseEvent(
        'click', {bubbles: true, cancelable: true, view: window}));

    document.body.removeChild(link);
  }

  /** @param {PolicyTableModel} dataModel */
  createOrUpdatePolicyTable(dataModel) {
    const id = `${dataModel.name}-${dataModel.id}`;
    if (!this.policyTables[id]) {
      this.policyTables[id] = document.createElement('policy-table');
      this.mainSection.appendChild(this.policyTables[id]);
    }
    this.policyTables[id].update(dataModel);
  }

  /**
   * Update the status section of the page to show the current cloud policy
   * status.
   * @param {Object} status Dictionary containing the current policy status.
   */
  setStatus(status) {
    // Remove any existing status boxes.
    const container = $('status-box-container');
    while (container.firstChild) {
      container.removeChild(container.firstChild);
    }
    // Hide the status section.
    const section = $('status-section');
    section.hidden = true;

    // Add a status box for each scope that has a cloud policy status.
    for (const scope in status) {
      const box = document.createElement('status-box');
      box.initialize(scope, status[scope]);
      container.appendChild(box);
      // Show the status section.
      section.hidden = false;
    }
  }

  /**
   * Re-enable the reload policies button when the previous request to reload
   * policies values has completed.
   */
  reloadPoliciesDone() {
    $('reload-policies').disabled = false;
    $('screen-reader-message').textContent =
        loadTimeData.getString('loadPoliciesDone');
  }
}

// Make Page a singleton.
addSingletonGetter(Page);

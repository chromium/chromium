// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './policy_precedence_row.js';
import './policy_row.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import {Policy, PolicyRowElement} from './policy_row.js';
import {getTemplate} from './policy_table.html.js';

export interface PolicyTableModel {
  id?: string;
  isExtension?: boolean;
  name: string;
  policies: NonNullable<Array<NonNullable<Policy>>>;
  precedenceOrder?: string[];
}

export class PolicyTableElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  filterPattern: string = '';

  update(dataModel: PolicyTableModel) {
    // Clear policies
    const mainContent = this.shadowRoot!.querySelector('.main');
    const policies = this.shadowRoot!.querySelectorAll('.policy-data');
    this.shadowRoot!.querySelector('.header')!.textContent = dataModel.name;
    this.shadowRoot!.querySelector('.id')!.textContent = dataModel.id || null;
    (this.shadowRoot!.querySelector('.id') as HTMLElement)!.hidden =
        !dataModel.id;
    policies.forEach(row => mainContent!.removeChild(row));

    dataModel.policies
        .sort((a, b) => {
          if ((a.value !== undefined && b.value !== undefined) ||
              a.value === b.value) {
            if (a.link !== undefined && b.link !== undefined) {
              // Sorting the policies in ascending alpha order.
              return a.name > b.name ? 1 : -1;
            }

            // Sorting so unknown policies are last.
            return a.link !== undefined ? -1 : 1;
          }

          // Sorting so unset values are last.
          return a.value !== undefined ? -1 : 1;
        })
        .forEach((policy: Policy) => {
          const policyRow: PolicyRowElement =
              document.createElement('policy-row');
          policyRow.initialize(policy);
          mainContent!.appendChild(policyRow);
        });
    this.filter();

    // Show the current policy precedence order in the Policy Precedence table.
    if (dataModel.name === 'Policy Precedence') {
      // Clear previous precedence row.
      const precedenceRowOld =
          this.shadowRoot!.querySelectorAll('.policy-precedence-data');
      precedenceRowOld.forEach(row => mainContent!.removeChild(row));
      if (dataModel.precedenceOrder != undefined) {
        const precedenceRow = document.createElement('policy-precedence-row');
        precedenceRow.initialize(dataModel.precedenceOrder);
        mainContent!.appendChild(precedenceRow);
      }
    }
  }

  /**
   * Set the filter pattern. Only policies whose name contains |pattern| are
   * shown in the policy table. The filter is case insensitive. It can be
   * disabled by setting |pattern| to an empty string.
   */
  setFilterPattern(pattern: string) {
    this.filterPattern = pattern.toLowerCase();
    this.filter();
  }

  /**
   * Filter policies. Only policies whose name contains the filter pattern are
   * shown in the table. Furthermore, policies whose value is not currently
   * set are only shown if the corresponding checkbox is checked.
   */
  filter() {
    const showUnset =
        (getRequiredElement('show-unset') as HTMLInputElement)!.checked;
    const policies = this.shadowRoot!.querySelectorAll('.policy-data');
    for (let i = 0; i < policies.length; i++) {
      const policyDisplay = policies[i] as PolicyRowElement;
      policyDisplay!.hidden =
          policyDisplay!.policy!.value === undefined && !showUnset ||
          policyDisplay!.policy!.name.toLowerCase().indexOf(
              this.filterPattern) === -1;
    }
    this.shadowRoot!.querySelector<HTMLElement>('.no-policy')!.hidden =
        !!this.shadowRoot!.querySelector('.policy-data:not([hidden])');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'policy-table': PolicyTableElement;
  }
}
customElements.define('policy-table', PolicyTableElement);

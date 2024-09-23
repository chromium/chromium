// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './policy_precedence_row.js';
import './policy_row.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import type {Policy, PolicyRowElement} from './policy_row.js';
import {getTemplate} from './policy_table.html.js';

export interface PolicyTableModel {
  id?: string;
  isExtension?: boolean;
  name: string;
  policies: NonNullable<Array<NonNullable<Policy>>>;
  precedenceOrder?: string[];
}

// Sortable columns/fields identifiers.
enum SortButtonsField {
  POLICY_NAME = 'name',
  POLICY_SOURCE = 'source',
  POLICY_SCOPE = 'scope',
  POLICY_LEVEL = 'level',
  POLICY_STATUS = 'status'
}

// The possible directions for sort.
enum SortOrder {
  ASCENDING = 1,
  DESCENDING = -1
}

export class PolicyTableElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  dataModel: PolicyTableModel;
  filterPattern: string = '';
  // The last sort order and column for the policy table.
  // These are used when policies are updated to prevent un-desired sort reset.
  mostRecentSortOrder: number = SortOrder.ASCENDING;
  mostRecentSortedColumn: string = SortButtonsField.POLICY_NAME;

  // Updates the data model and table.
  updateDataModel(dataModel: PolicyTableModel) {
    this.dataModel = dataModel;
    // Update table based on the updated data model.
    this.update();
  }

  addEventListeners() {
    for (const field of Object.values(SortButtonsField)) {
      const sortUpButton = this.getRequiredElement(`#${field}-sort-up`);
      const sortDownButton = this.getRequiredElement(`#${field}-sort-down`);
      sortUpButton.onclick = () => this.update(SortOrder.ASCENDING, field);
      sortDownButton.onclick = () => this.update(SortOrder.DESCENDING, field);
    }
  }

  update(
      order: number = this.mostRecentSortOrder,
      field: string = this.mostRecentSortedColumn) {
    // Clear policies
    const mainContent = this.getRequiredElement('.main');
    const policies = this.shadowRoot!.querySelectorAll('.policy-data');
    this.getRequiredElement('.header').textContent = this.dataModel.name;
    this.getRequiredElement('.id').textContent = this.dataModel.id || null;
    this.getRequiredElement('.id').hidden = !this.dataModel.id;
    policies.forEach(row => mainContent.removeChild(row));

    this.dataModel.policies
        .sort((a, b) => {
          // Save most recent sort preference.
          this.mostRecentSortOrder = order;
          this.mostRecentSortedColumn = field;
          if ((a.value !== undefined && b.value !== undefined) ||
              a.value === b.value) {
            if (a.link !== undefined && b.link !== undefined) {
              // Sorting the policies in chosen alpha order based on the field
              // selected, with secondary sort based on Policy name.
              if (field !== SortButtonsField.POLICY_NAME &&
                  a[field as keyof Policy] === b[field as keyof Policy]) {
                return order *
                    (a[SortButtonsField.POLICY_NAME] >
                             b[SortButtonsField.POLICY_NAME] ?
                         1 :
                         -1);
              }
              return order *
                  (a[field as keyof Policy] > b[field as keyof Policy] ? 1 :
                                                                         -1);
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
          mainContent.appendChild(policyRow);
        });
    this.filter();

    // Show the current policy precedence order in the Policy Precedence table.
    if (this.dataModel.name === 'Policy Precedence') {
      // Clear previous precedence row.
      const precedenceRowOld =
          this.shadowRoot!.querySelectorAll('.policy-precedence-data');
      precedenceRowOld.forEach(row => mainContent.removeChild(row));
      if (this.dataModel.precedenceOrder != undefined) {
        const precedenceRow = document.createElement('policy-precedence-row');
        precedenceRow.initialize(this.dataModel.precedenceOrder);
        mainContent.appendChild(precedenceRow);
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
    this.getRequiredElement<HTMLElement>('.no-policy').hidden =
        !!this.shadowRoot!.querySelector('.policy-data:not([hidden])');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'policy-table': PolicyTableElement;
  }
}
customElements.define('policy-table', PolicyTableElement);

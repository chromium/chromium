// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import './policy_precedence_row.js';
import './policy_row.js';
import './policy_table_header_cell.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PolicyUiModel} from './policy_row.js';
import {getCss} from './policy_table.css.js';
import {getHtml} from './policy_table.html.js';

export interface PolicyTableModel {
  id?: string;
  isExtension?: boolean;
  name: string;
  policies: NonNullable<Array<NonNullable<PolicyUiModel>>>;
  precedenceOrder?: string[];
}

export class PolicyTableElement extends CrLitElement {
  static get is() {
    return 'policy-table';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      dataModel: {type: Object},
      filterPattern: {type: String},
      showUnset: {type: Boolean},
      sortAscending: {type: Boolean},
      mostRecentSortedColumn: {type: String},
      sortedPolicies: {type: Array},
    };
  }

  accessor dataModel: PolicyTableModel|null = null;
  accessor filterPattern: string = '';
  accessor showUnset: boolean = false;
  accessor sortAscending: boolean = true;
  accessor mostRecentSortedColumn: keyof PolicyUiModel = 'name';
  accessor sortedPolicies: PolicyUiModel[] = [];
  updateDataModel(dataModel: PolicyTableModel) {
    this.dataModel = dataModel;
  }

  setFilterPattern(pattern: string) {
    this.filterPattern = pattern.toLowerCase();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('dataModel') ||
        changedProperties.has('sortAscending') ||
        changedProperties.has('mostRecentSortedColumn')) {
      this.sortedPolicies = this.getSortedPolicies();
    }
  }

  protected sortColumn(ascending: boolean, field: keyof PolicyUiModel) {
    this.sortAscending = ascending;
    this.mostRecentSortedColumn = field;
  }

  protected onSortChanged(
      e: CustomEvent<{field: keyof PolicyUiModel, ascending: boolean}>) {
    this.sortColumn(e.detail.ascending, e.detail.field);
  }

  protected getSortedPolicies(): PolicyUiModel[] {
    if (!this.dataModel || !this.dataModel.policies) {
      return [];
    }

    const sorted = [...this.dataModel.policies];
    const orderMultiplier = this.sortAscending ? 1 : -1;
    const field = this.mostRecentSortedColumn;

    sorted.sort((a, b) => {
      if ((a.value !== undefined && b.value !== undefined) ||
          a.value === b.value) {
        if (a.link !== undefined && b.link !== undefined) {
          if (field !== 'name' && a[field] === b[field]) {
            return orderMultiplier * (a.name > b.name ? 1 : -1);
          }
          return orderMultiplier *
              ((a[field] as string) > (b[field] as string) ? 1 : -1);
        }
        return a.link !== undefined ? -1 : 1;
      }
      return a.value !== undefined ? -1 : 1;
    });

    return sorted;
  }

  protected isPolicyHidden(policy: PolicyUiModel): boolean {
    const matchesSearch =
        policy.name.toLowerCase().includes(this.filterPattern);
    const isSet = policy.value !== undefined;
    return !(matchesSearch && (isSet || this.showUnset));
  }

  protected hasVisiblePolicies(): boolean {
    return this.sortedPolicies.some(policy => !this.isPolicyHidden(policy));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'policy-table': PolicyTableElement;
  }
}

customElements.define(PolicyTableElement.is, PolicyTableElement);

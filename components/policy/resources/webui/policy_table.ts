// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import './policy_precedence_row.js';
import './policy_row.js';
import './policy_table_header_cell.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {NAME_COLUMN, OPTIONAL_POLICY_COLUMNS, OptionalPolicyColumn, TOGGLE_COLUMN} from './policy_columns.js';
import type {PolicyUiModel} from './policy_row.js';
import {getCss} from './policy_table.css.js';
import {getHtml} from './policy_table.html.js';

const COLUMN_TRACK_SIZES: Record<OptionalPolicyColumn, string> = {
  [OptionalPolicyColumn.VALUE]: 'minmax(100px, 3fr)',
  [OptionalPolicyColumn.SOURCE]: 'minmax(72px, 1fr)',
  [OptionalPolicyColumn.SCOPE]: 'minmax(72px, 1fr)',
  [OptionalPolicyColumn.LEVEL]: 'minmax(72px, 1fr)',
  [OptionalPolicyColumn.STATUS]: 'minmax(72px, 1fr)',
};

const COLUMN_MIN_WIDTHS_PX: Record<OptionalPolicyColumn, number> = {
  [OptionalPolicyColumn.VALUE]: 100,
  [OptionalPolicyColumn.SOURCE]: 72,
  [OptionalPolicyColumn.SCOPE]: 72,
  [OptionalPolicyColumn.LEVEL]: 72,
  [OptionalPolicyColumn.STATUS]: 72,
};

const NAME_MIN_WIDTH_PX = 100;
const TOGGLE_MIN_WIDTH_PX = 48;

export interface PolicyTableModel {
  id?: string;
  isExtension?: boolean;
  isWebApp?: boolean;
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
      visibleColumns: {type: Array},
    };
  }

  accessor dataModel: PolicyTableModel|null = null;
  accessor filterPattern: string = '';
  accessor showUnset: boolean = false;
  accessor sortAscending: boolean = true;
  accessor mostRecentSortedColumn: keyof PolicyUiModel = NAME_COLUMN;
  accessor sortedPolicies: PolicyUiModel[] = [];
  accessor visibleColumns: OptionalPolicyColumn[] =
      [...OPTIONAL_POLICY_COLUMNS];

  updateDataModel(dataModel: PolicyTableModel) {
    this.dataModel = dataModel;
  }

  setFilterPattern(pattern: string) {
    this.filterPattern = pattern.toLowerCase();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    // Ensure sorting happens on a column that is visible; fall back to `name`.
    let sortReset = false;
    if (changedProperties.has('visibleColumns') &&
        this.mostRecentSortedColumn !== NAME_COLUMN &&
        !this.isColumnVisible(
            this.mostRecentSortedColumn as OptionalPolicyColumn)) {
      this.mostRecentSortedColumn = NAME_COLUMN;
      this.sortAscending = true;
      sortReset = true;
    }
    if (sortReset || changedProperties.has('dataModel') ||
        changedProperties.has('sortAscending') ||
        changedProperties.has('mostRecentSortedColumn')) {
      this.sortedPolicies = this.getSortedPolicies();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('visibleColumns')) {
      // `<policy-table>`, `<policy-row>`, and `<policy-conflict>` render in
      // separate Shadow DOM trees, so standard CSS selectors cannot share a
      // dynamic grid layout across them. Setting inherited CSS custom
      // properties on the `<policy-table>` host propagates a single CSS Grid
      // track list (`--policy-grid-template-columns`), named area map
      // (`--policy-grid-template-areas`), and minimum row width
      // (`--policy-row-min-width`) into all descendant rows so hidden columns
      // collapse out of the grid while visible columns stay aligned.
      const activeColumns = OPTIONAL_POLICY_COLUMNS.filter(
          column => this.visibleColumns.includes(column));
      const templateColumns = [
        'minmax(100px, 2.5fr)',
        ...activeColumns.map(column => COLUMN_TRACK_SIZES[column]),
        'minmax(48px, 5%)',
      ].join(' ');
      const templateAreas =
          `'${[NAME_COLUMN, ...activeColumns, TOGGLE_COLUMN].join(' ')}'`;
      const minWidthPx = activeColumns.reduce(
          (sum, column) => sum + COLUMN_MIN_WIDTHS_PX[column],
          NAME_MIN_WIDTH_PX + TOGGLE_MIN_WIDTH_PX);
      this.style.setProperty('--policy-grid-template-columns', templateColumns);
      this.style.setProperty('--policy-grid-template-areas', templateAreas);
      this.style.setProperty('--policy-row-min-width', `${minWidthPx}px`);
    }
  }

  protected isColumnVisible(column: OptionalPolicyColumn): boolean {
    return this.visibleColumns.includes(column);
  }

  private sortColumn(ascending: boolean, field: keyof PolicyUiModel) {
    this.sortAscending = ascending;
    this.mostRecentSortedColumn = field;
  }

  protected onSortChanged(
      e: CustomEvent<{field: keyof PolicyUiModel, ascending: boolean}>) {
    this.sortColumn(e.detail.ascending, e.detail.field);
  }

  private getSortedPolicies(): PolicyUiModel[] {
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
          if (field !== NAME_COLUMN && a[field] === b[field]) {
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

  protected shouldShowId(): boolean {
    if (!this.dataModel) {
      return false;
    }
    return !!this.dataModel.isExtension ||
        (!!this.dataModel.isWebApp && !!this.dataModel.name);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'policy-table': PolicyTableElement;
  }
}

customElements.define(PolicyTableElement.is, PolicyTableElement);

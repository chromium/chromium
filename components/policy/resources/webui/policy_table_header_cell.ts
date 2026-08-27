// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PolicyUiModel} from './policy_row.js';
import {getCss} from './policy_table_header_cell.css.js';
import {getHtml} from './policy_table_header_cell.html.js';

export class PolicyTableHeaderCellElement extends CrLitElement {
  static get is() {
    return 'policy-table-header-cell';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      field: {type: String},
      headerTitle: {type: String},
      sortedColumn: {type: String},
      sortAscending: {type: Boolean},
      noSort: {type: Boolean},
    };
  }

  accessor field: keyof PolicyUiModel = 'name';
  accessor headerTitle: string = '';
  accessor sortedColumn: keyof PolicyUiModel = 'name';
  accessor sortAscending: boolean = true;
  accessor noSort: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.setAttribute('role', 'columnheader');
    this.updateAriaSort();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('sortedColumn') ||
        changedProperties.has('sortAscending') ||
        changedProperties.has('field') ||
        changedProperties.has('headerTitle')) {
      this.updateAriaSort();
    }
    if (changedProperties.has('sortedColumn') ||
        changedProperties.has('field')) {
      if (this.sortedColumn === this.field) {
        this.setAttribute('selected', '');
      } else {
        this.removeAttribute('selected');
      }
    }
  }

  private updateAriaSort() {
    if (this.sortedColumn !== this.field) {
      this.setAttribute('aria-sort', 'none');
    } else {
      this.setAttribute(
          'aria-sort', this.sortAscending ? 'ascending' : 'descending');
    }
    this.setAttribute('aria-label', this.headerTitle);
  }

  protected onSortUpClick() {
    this.fire('sort-changed', {
      field: this.field,
      ascending: true,
    });
  }

  protected onSortDownClick() {
    this.fire('sort-changed', {
      field: this.field,
      ascending: false,
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'policy-table-header-cell': PolicyTableHeaderCellElement;
  }
}

customElements.define(
    PolicyTableHeaderCellElement.is, PolicyTableHeaderCellElement);

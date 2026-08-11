// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './policy_conflict.css.js';
import {getHtml} from './policy_conflict.html.js';

export interface Conflict {
  level: string;
  scope: string;
  source: string;
  value: unknown;
}

// Converts a policy value to a JSON string and optionally formats it.
export function stringifyPolicyValue(value: unknown, format?: boolean): string {
  // Guard against undefined values;
  // pass nulls, as they are a valid policy value.
  if (value === undefined) {
    return '';
  }
  // Skip 'string' policy to avoid unnecessary conversions.
  if (typeof value === 'string') {
    return value;
  }
  if (format) {
    return JSON.stringify(value, null, 2);
  } else {
    return JSON.stringify(value, null);
  }
}

// Copies the text content of an element to the clipboard.
export function copyValue(element: HTMLElement) {
  const selection = window.getSelection();
  const range = window.document.createRange();
  range.selectNodeContents(element);
  selection!.removeAllRanges();
  selection!.addRange(range);

  navigator.clipboard.writeText(element.innerText).catch(error => {
    console.error('Unable to copy value to clipboard:', error);
  });
}



export class PolicyConflictElement extends CrLitElement {
  static get is() {
    return 'policy-conflict';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      conflict: {type: Object},
      rowLabel: {type: String},
      policyName: {type: String},
    };
  }

  accessor conflict: Conflict|null = null;
  accessor rowLabel: string = '';
  accessor policyName: string = '';

  override connectedCallback() {
    super.connectedCallback();
    this.setAttribute('role', 'rowgroup');
  }



  protected getRowLabelText(): string {
    return this.rowLabel ? loadTimeData.getString(this.rowLabel) : '';
  }

  protected getScopeText(): string {
    if (!this.conflict) {
      return '';
    }
    return loadTimeData.getString(
        this.conflict.scope === 'user' ? 'scopeUser' : 'scopeDevice');
  }

  protected getLevelText(): string {
    if (!this.conflict) {
      return '';
    }
    return loadTimeData.getString(
        this.conflict.level === 'recommended' ? 'levelRecommended' :
                                                'levelMandatory');
  }

  protected getSourceText(): string {
    if (!this.conflict) {
      return '';
    }
    return loadTimeData.getString(this.conflict.source);
  }

  protected getFormattedValue(): string {
    if (!this.conflict) {
      return '';
    }
    return stringifyPolicyValue(this.conflict.value, /*format=*/ true);
  }

  protected getCopyLabel(): string {
    return loadTimeData.getStringF('policyCopyValue', this.policyName);
  }

  // Copies the policy's conflicting/superseded value to the clipboard.
  protected onCopyClick(e: Event) {
    const target = e.currentTarget as HTMLElement;
    // Walk up the DOM to find the parent .row and then get the .value element.
    const row = target.closest('.row') || target.closest('.entry');
    const valueDisplay = row?.querySelector('.value');
    if (valueDisplay) {
      copyValue(valueDisplay as HTMLElement);
    }
  }

  protected getCopyIconSrc(): string {
    return document.documentElement.hasAttribute('webui-rounded-icons') ?
        'chrome://resources/images/icon_copy_content.svg' :
        'chrome://resources/images/icon_copy_content_old.svg';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'policy-conflict': PolicyConflictElement;
  }
}

customElements.define(PolicyConflictElement.is, PolicyConflictElement);

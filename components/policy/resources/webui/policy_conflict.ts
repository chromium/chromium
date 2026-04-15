// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {getTemplate} from './policy_conflict.html.js';

export interface Conflict {
  level: string;
  scope: string;
  source: string;
  value: any;
}

// Converts a policy value to a JSON string and optionally formats it.
export function stringifyPolicyValue(value: any, format?: boolean): string {
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
export function copyValue(element: CustomElement) {
  const selection = window.getSelection();
  const range = window.document.createRange();
  range.selectNodeContents(element);
  selection!.removeAllRanges();
  selection!.addRange(range);

  navigator.clipboard.writeText(element.innerText).catch(error => {
    console.error('Unable to copy value to clipboard:', error);
  });
}

export class PolicyConflictElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  connectedCallback() {
    this.toggleAttribute('hidden', true);
    this.setAttribute('role', 'rowgroup');

    const copyLink = this.shadowRoot!.querySelector('.value.row .copy-value');
    if (copyLink) {
      copyLink.addEventListener('click', () => this.copyValue_());
    }
  }

  initialize(conflict: Conflict, rowLabel: string, _policyName: string) {
    const scopeText = loadTimeData.getString(
        conflict.scope === 'user' ? 'scopeUser' : 'scopeDevice');
    const levelText = loadTimeData.getString(
        conflict.level === 'recommended' ? 'levelRecommended' :
                                           'levelMandatory');
    const sourceText = loadTimeData.getString(conflict.source);
    const valueText = stringifyPolicyValue(conflict.value, /*format=*/ true);
    const nameText = loadTimeData.getString(rowLabel);

    const setText = (selector: string, text: string) => {
      this.shadowRoot!.querySelector(selector)!.textContent = text;
    };

    setText('.scope', scopeText);
    setText('.level', levelText);
    setText('.source', sourceText);
    setText('.value', valueText);
    setText('.name', nameText);

    // Populate the mobile-specific layout elements.
    // On space-constrained devices, conflicts are displayed as a
    // vertical stack of rows instead of a single horizontal row with
    // columns.
    // <if expr="is_android or is_ios">
    setText('.value.row .name', nameText);
    setText('.value.row .value', valueText);
    setText('.source.row .value', sourceText);
    setText('.scope.row .value', scopeText);
    setText('.level.row .value', levelText);

    // Set the label for the copy link.
    const copyLink = this.shadowRoot!.querySelector('.value.row .copy-value');
    if (copyLink) {
      const copyLabel = loadTimeData.getStringF('policyCopyValue', _policyName);
      copyLink.setAttribute('title', copyLabel);
      copyLink.setAttribute('aria-label', copyLabel);
    }
    // </if>
  }

  // Copies the policy's conflicting/superseded value to the clipboard.
  private copyValue_() {
    const valueDisplay = this.shadowRoot!.querySelector('.value.row .value');
    if (valueDisplay) {
      copyValue(valueDisplay as CustomElement);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'policy-conflict': PolicyConflictElement;
  }
}

customElements.define('policy-conflict', PolicyConflictElement);

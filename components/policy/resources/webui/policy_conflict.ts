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

export class PolicyConflictElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  connectedCallback() {
    this.toggleAttribute('hidden', true);
    this.setAttribute('role', 'rowgroup');
  }

  initialize(conflict: Conflict, rowLabel: string) {
    const scopeText = loadTimeData.getString(
        conflict.scope === 'user' ? 'scopeUser' : 'scopeDevice');
    const levelText = loadTimeData.getString(
        conflict.level === 'recommended' ? 'levelRecommended' :
                                           'levelMandatory');
    const sourceText = loadTimeData.getString(conflict.source);
    const valueText = JSON.stringify(conflict.value);
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
    // </if>
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'policy-conflict': PolicyConflictElement;
  }
}

customElements.define('policy-conflict', PolicyConflictElement);

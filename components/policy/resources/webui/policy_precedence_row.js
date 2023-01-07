// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './policy_precedence_row.html.js';

export class PolicyPrecedenceRowElement extends CustomElement {
  static get template() {
    return getTemplate();
  }

  connectedCallback() {
    this.setAttribute('role', 'rowgroup');
    this.classList.add('policy-precedence-data');
  }

  /**
   * @param {Array<string>} precedenceOrder Array containing ordered strings
   *     which represent the order of policy precedence.
   */
  initialize(precedenceOrder) {
    this.shadowRoot.querySelector('.precedence.row > .value').textContent =
        precedenceOrder.join(' > ');
  }
}

customElements.define('policy-precedence-row', PolicyPrecedenceRowElement);

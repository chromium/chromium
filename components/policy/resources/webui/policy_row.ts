// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/action_link.js';
import './policy_conflict.js';
import '/strings.m.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import type {Conflict} from './policy_conflict.js';
import {copyValue, setCopyButtonAccessibilityAttributes, stringifyPolicyValue} from './policy_conflict.js';
import {getTemplate} from './policy_row.html.js';

export interface Policy {
  ignored?: boolean;
  name: string;
  level: string;
  link?: string;
  scope: string;
  source: string;
  error: string;
  warning: string;
  info: string;
  value: unknown;
  deprecated?: boolean;
  future?: boolean;
  allSourcesMerged?: boolean;
  conflicts?: Conflict[];
  superseded?: Conflict[];
  forSigninScreen: boolean;
  isExtension: boolean;
  status: string;
}


export class PolicyRowElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  policy: Policy;
  private unset_: boolean;
  private hasErrors_: boolean;
  private hasWarnings_: boolean;
  private hasInfos_: boolean;
  private hasConflicts_: boolean;
  private hasSuperseded_: boolean;
  private isMergedValue_: boolean;
  private deprecated_: boolean;
  private future_: boolean;

  connectedCallback() {
    const toggle = this.shadowRoot!.querySelector('.policy.row .toggle');
    toggle!.addEventListener('click', () => this.toggleExpanded());

    const copyButton = this.shadowRoot!.querySelector('.copy-value');
    if (copyButton) {
      copyButton.addEventListener('click', () => this.copyValue_());
    }

    this.setAttribute('role', 'rowgroup');
    this.classList.add('policy-data');
  }

  initialize(policy: Policy) {
    this.policy = policy;

    this.unset_ = policy.value === undefined;

    this.hasErrors_ = !!policy.error;

    this.hasWarnings_ = !!policy.warning;

    this.hasInfos_ = !!policy.info;

    this.hasConflicts_ = !!policy.conflicts;

    this.hasSuperseded_ = !!policy.superseded;

    this.isMergedValue_ = !!policy.allSourcesMerged;

    this.deprecated_ = !!policy.deprecated;

    this.future_ = !!policy.future;

    // Populate the name column.
    const nameDisplay = this.shadowRoot!.querySelector('.name .link span');
    nameDisplay!.textContent = policy.name;
    if (policy.link) {
      const link = this.getRequiredElement<HTMLAnchorElement>('.name .link');
      link.href = policy.link;
      link.title = loadTimeData.getStringF('policyLearnMore', policy.name);
      this.toggleAttribute('no-help-link', false);
    } else {
      this.toggleAttribute('no-help-link', true);
    }

    // Populate the remaining columns with policy scope, level and value if a
    // value has been set. Otherwise, leave them blank and fill only the status
    // value with the "unset" notice.

    if (this.unset_) {
      const notice = loadTimeData.getString('unset');
      const messagesDisplay = this.shadowRoot!.querySelector('.messages');
      messagesDisplay!.textContent = notice;

      // Remove the copy button since there is no value to copy.
      const copyButton = this.shadowRoot!.querySelector('.copy .link');
      if (copyButton) {
        copyButton.remove();
      }

      // On space constraint devices, status is displayed as a row.
      // <if expr="is_android or is_ios">
      const messagesRowContentDisplay =
          this.shadowRoot!.querySelector('.messages.row .value');
      messagesRowContentDisplay!.textContent = notice;
      // </if>
      return;
    }

    const scopeDisplay = this.shadowRoot!.querySelector('.scope');
    let scope = 'scopeDevice';
    if (policy.scope === 'user') {
      scope = 'scopeUser';
    } else if (policy.scope === 'allUsers') {
      scope = 'scopeAllUsers';
    }
    scopeDisplay!.textContent = loadTimeData.getString(scope);

    const levelDisplay = this.shadowRoot!.querySelector('.level');
    levelDisplay!.textContent = loadTimeData.getString(
        policy.level === 'recommended' ? 'levelRecommended' : 'levelMandatory');

    const sourceDisplay = this.shadowRoot!.querySelector('.source');
    sourceDisplay!.textContent = loadTimeData.getString(policy.source);

    // Reduces load on the DOM for long values;
    // If value is longer than 256 characters, truncate and add ellipsis.
    const policyValueStr = stringifyPolicyValue(policy.value);
    const truncatedValue = policyValueStr.length > 256 ?
        `${policyValueStr.substring(0, 256)}\u2026` :
        policyValueStr;

    const valueDisplay = this.shadowRoot!.querySelector('.value');
    valueDisplay!.textContent = truncatedValue;

    // Set the label for the copy button.
    const copyLink = this.shadowRoot!.querySelector('.copy .link');
    if (copyLink) {
      setCopyButtonAccessibilityAttributes(copyLink, policy.name);
    }

    const valueRowContentDisplay =
        this.shadowRoot!.querySelector('.value.row .value');
    // Expanded policy value is formatted.
    valueRowContentDisplay!.textContent =
        stringifyPolicyValue(policy.value, /*format=*/ true);

    const errorRowContentDisplay =
        this.shadowRoot!.querySelector('.errors.row .value');
    errorRowContentDisplay!.textContent = policy.error;
    const warningRowContentDisplay =
        this.shadowRoot!.querySelector('.warnings.row .value');
    warningRowContentDisplay!.textContent = policy.warning;
    const infoRowContentDisplay =
        this.shadowRoot!.querySelector('.infos.row .value');
    infoRowContentDisplay!.textContent = policy.info;

    const messagesDisplay = this.shadowRoot!.querySelector('.messages');
    const errorsNotice = this.hasErrors_ ? loadTimeData.getString('error') : '';
    const deprecationNotice =
        this.deprecated_ ? loadTimeData.getString('deprecated') : '';
    const futureNotice = this.future_ ? loadTimeData.getString('future') : '';
    const warningsNotice =
        this.hasWarnings_ ? loadTimeData.getString('warning') : '';
    const conflictsNotice = this.hasConflicts_ && !this.isMergedValue_ ?
        loadTimeData.getString('conflict') :
        '';
    const ignoredNotice = this.policy.ignored ?
        (this.policy.isExtension ?
             loadTimeData.getString('ignoredByExtension') :
             loadTimeData.getString('ignored')) :
        '';
    let notice =
        [
          errorsNotice,
          deprecationNotice,
          futureNotice,
          warningsNotice,
          ignoredNotice,
          conflictsNotice,
        ].filter(x => !!x)
            .join(', ') ||
        loadTimeData.getString('ok');
    const supersededNotice = this.hasSuperseded_ && !this.isMergedValue_ ?
        loadTimeData.getString('superseding') :
        '';
    if (supersededNotice) {
      // Include superseded notice regardless of other notices
      notice += `, ${supersededNotice}`;
    }
    messagesDisplay!.textContent = notice;
    policy.status = notice;

    // Display scope, level and status as rows instead of columns on space
    // constraint devices.
    // <if expr="is_android or is_ios">
    const scopeRowContentDisplay =
        this.shadowRoot!.querySelector('.scope.row .value');
    scopeRowContentDisplay!.textContent = loadTimeData.getString(scope);
    const levelRowContentDisplay =
        this.shadowRoot!.querySelector('.level.row .value');
    levelRowContentDisplay!.textContent = loadTimeData.getString(
        policy.level === 'recommended' ? 'levelRecommended' : 'levelMandatory');
    const messagesRowContentDisplay =
        this.shadowRoot!.querySelector('.messages.row .value');
    messagesRowContentDisplay!.textContent = notice;
    // </if>

    if (policy.conflicts) {
      policy.conflicts.forEach(conflict => {
        const row = document.createElement('policy-conflict');
        row.initialize(conflict, 'conflictValue', this.policy.name);
        row.classList.add('policy-conflict-data');
        this.shadowRoot!.appendChild(row);
      });
    }
    if (policy.superseded) {
      policy.superseded.forEach(superseded => {
        const row = document.createElement('policy-conflict');
        row.initialize(superseded, 'supersededValue', this.policy.name);
        row.classList.add('policy-superseded-data');
        this.shadowRoot!.appendChild(row);
      });
    }
  }

  // Copies the policy's value to the clipboard.
  private copyValue_() {
    const policyValueDisplay =
        this.shadowRoot!.querySelector('.value.row .value');
    if (policyValueDisplay) {
      copyValue(policyValueDisplay as CustomElement);
    }
  }

  // Toggle the visibility of an additional row containing the complete text.
  private toggleExpanded() {
    const warningRowDisplay = this.getRequiredElement('.warnings.row');
    const errorRowDisplay = this.getRequiredElement('.errors.row');
    const infoRowDisplay = this.getRequiredElement('.infos.row');
    const valueRowDisplay = this.getRequiredElement('.value.row');
    // <if expr="is_android or is_ios">
    const scopeRowDisplay = this.getRequiredElement('.scope.row');
    scopeRowDisplay.hidden = !scopeRowDisplay.hidden;
    const levelRowDisplay = this.getRequiredElement('.level.row');
    levelRowDisplay.hidden = !levelRowDisplay.hidden;
    const messagesRowDisplay = this.getRequiredElement('.messages.row');
    messagesRowDisplay.hidden = !messagesRowDisplay.hidden;
    // </if>
    valueRowDisplay.hidden = !valueRowDisplay.hidden;
    this.classList.toggle('expanded', !valueRowDisplay.hidden);

    this.getRequiredElement('.show-more').hidden = !valueRowDisplay.hidden;
    this.getRequiredElement('.show-less').hidden = valueRowDisplay.hidden;
    if (this.hasWarnings_) {
      warningRowDisplay.hidden = !warningRowDisplay.hidden;
    }
    if (this.hasErrors_) {
      errorRowDisplay.hidden = !errorRowDisplay.hidden;
    }
    if (this.hasInfos_) {
      infoRowDisplay.hidden = !infoRowDisplay.hidden;
    }
    this.shadowRoot!.querySelectorAll<HTMLElement>('.policy-conflict-data')
        .forEach(row => row.hidden = !row.hidden);
    this.shadowRoot!.querySelectorAll<HTMLElement>('.policy-superseded-data')
        .forEach(row => row.hidden = !row.hidden);
  }
}
declare global {
  interface HTMLElementTagNameMap {
    'policy-row': PolicyRowElement;
  }
}
customElements.define('policy-row', PolicyRowElement);

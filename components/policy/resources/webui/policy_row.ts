// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './policy_conflict.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Conflict} from './policy_conflict.js';
import {copyValue, stringifyPolicyValue} from './policy_conflict.js';
import {getCss} from './policy_row.css.js';
import {getHtml} from './policy_row.html.js';

export interface ConflictItem {
  conflict: Conflict;
  label: string;
  className: string;
}

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
  restartRequired?: boolean;
  deprecated?: boolean;
  future?: boolean;
  allSourcesMerged?: boolean;
  conflicts?: Conflict[];
  superseded?: Conflict[];
  forSigninScreen: boolean;
  isExtension: boolean;
  status: string;
}

export class PolicyRowElement extends CrLitElement {
  static get is() {
    return 'policy-row';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      policy: {type: Object},
      expanded: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor policy: Policy|null = null;
  accessor expanded: boolean = false;

  protected get unset(): boolean {
    return !!(this.policy && this.policy.value === undefined);
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setAttribute('role', 'rowgroup');
    this.classList.add('policy-data');
    this.expanded = false;
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('policy') && this.policy) {
      this.policy.status = this.computeStatusText();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('policy')) {
      if (this.policy && this.policy.link) {
        this.removeAttribute('no-help-link');
      } else {
        this.setAttribute('no-help-link', '');
      }
    }
  }



  protected getShowMoreLessLabel(): string {
    return loadTimeData.getString(this.expanded ? 'showLess' : 'showMore');
  }

  protected getLearnMoreTooltip(): string {
    return this.policy?.name ?
        loadTimeData.getStringF('policyLearnMore', this.policy.name) :
        '';
  }

  protected getCopyLabel(): string {
    return this.policy?.name ?
        loadTimeData.getStringF('policyCopyValue', this.policy.name) :
        '';
  }

  protected getScopeText(): string {
    if (!this.policy || this.unset) {
      return '';
    }
    const scopeMap: Record<string, string> = {
      'user': 'scopeUser',
      'allUsers': 'scopeAllUsers',
    };
    return loadTimeData.getString(scopeMap[this.policy.scope] || 'scopeDevice');
  }

  protected getLevelText(): string {
    if (!this.policy || this.unset) {
      return '';
    }
    return loadTimeData.getString(
        this.policy.level === 'recommended' ? 'levelRecommended' :
                                              'levelMandatory');
  }

  protected getSourceText(): string {
    if (!this.policy || this.unset) {
      return '';
    }
    return loadTimeData.getString(this.policy.source);
  }

  protected getTruncatedValue(): string {
    if (!this.policy || this.unset) {
      return '';
    }
    const policyValueStr = stringifyPolicyValue(this.policy.value);
    return policyValueStr.length > 256 ?
        `${policyValueStr.substring(0, 256)}\u2026` :
        policyValueStr;
  }

  protected getFormattedValue(): string {
    if (!this.policy || this.unset) {
      return '';
    }
    return stringifyPolicyValue(this.policy.value, /*format=*/ true);
  }

  protected getMessagesText(): string {
    return this.policy?.status || '';
  }

  private computeStatusText(): string {
    if (!this.policy) {
      return '';
    }
    if (this.unset) {
      return loadTimeData.getString('unset');
    }

    const {
      error,
      warning,
      conflicts,
      superseded,
      allSourcesMerged,
      ignored,
      isExtension,
      deprecated,
      future,
      restartRequired,
    } = this.policy;
    const isMergedValue = !!allSourcesMerged;

    const notices = [
      error ? loadTimeData.getString('error') : '',
      deprecated ? loadTimeData.getString('deprecated') : '',
      future ? loadTimeData.getString('future') : '',
      warning ? loadTimeData.getString('warning') : '',
      restartRequired ? loadTimeData.getString('restartRequired') : '',
      ignored ? loadTimeData.getString(
                    isExtension ? 'ignoredByExtension' : 'ignored') :
                '',
      (conflicts?.length && !isMergedValue) ? loadTimeData.getString('conflict') : '',
    ].filter(Boolean);

    let notice = notices.join(', ') || loadTimeData.getString('ok');

    if (superseded?.length && !isMergedValue) {
      notice += `, ${loadTimeData.getString('superseding')}`;
    }

    return notice;
  }

  protected get conflictItems(): ConflictItem[] {
    return [
      ...(this.policy?.conflicts || []).map(c => ({
                                              conflict: c,
                                              label: 'conflictValue',
                                              className: 'policy-conflict-data',
                                            })),
      ...(this.policy?.superseded || []).map(c => ({
                                               conflict: c,
                                               label: 'supersededValue',
                                               className:
                                                   'policy-superseded-data',
                                             })),
    ];
  }

  // Copies the policy's value to the clipboard.
  protected onCopyClick(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const row = target.closest('.row');
    const policyValueDisplay = row?.querySelector('.value');
    if (policyValueDisplay) {
      copyValue(policyValueDisplay as HTMLElement);
    }
  }

  // Toggle the visibility of an additional row containing the complete text.
  protected onToggleExpandedClick() {
    this.expanded = !this.expanded;
  }

  protected getOpenInNewIconSrc(): string {
    return document.documentElement.hasAttribute('webui-rounded-icons') ?
        'chrome://resources/images/open_in_new.svg' :
        'chrome://resources/images/open_in_new_old.svg';
  }

  protected getCopyIconSrc(): string {
    return document.documentElement.hasAttribute('webui-rounded-icons') ?
        'chrome://resources/images/icon_copy_content.svg' :
        'chrome://resources/images/icon_copy_content_old.svg';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'policy-row': PolicyRowElement;
  }
}

customElements.define(PolicyRowElement.is, PolicyRowElement);

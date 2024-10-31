// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {CommerceInternalsApiProxy} from './commerce_internals_api_proxy.js';
import {getCss} from './eligibility_list.css.js';
import {getHtml} from './eligibility_list.html.js';

interface EligibilityDetail {
  name: string;
  value: boolean;
  expectedValue: boolean;
}

export class EligibilityListElement extends CrLitElement {
  static get is() {
    return 'commerce-internals-eligibility-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      details_: {type: Array},
    };
  }

  protected details_: EligibilityDetail[] = [];

  private commerceInternalsApi_: CommerceInternalsApiProxy =
      CommerceInternalsApiProxy.getInstance();

  constructor() {
    super();

    this.commerceInternalsApi_.getCallbackRouter()
        .onShoppingListEligibilityChanged.addListener(() => {
          this.refreshDetails_();
        });
  }

  override connectedCallback() {
    super.connectedCallback();
    this.refreshDetails_();
  }

  protected async refreshDetails_() {
    const detail =
        (await this.commerceInternalsApi_.getShoppingListEligibleDetails())
            .detail;

    this.details_ = [];
    this.details_.push({
      name: 'isRegionLockedFeatureEnabled',
      value: detail.isRegionLockedFeatureEnabled.value,
      expectedValue: detail.isRegionLockedFeatureEnabled.expectedValue,
    });
    this.details_.push({
      name: 'isShoppingListAllowedForEnterprise',
      value: detail.isShoppingListAllowedForEnterprise.value,
      expectedValue: detail.isShoppingListAllowedForEnterprise.expectedValue,
    });
    this.details_.push({
      name: 'IsAccountCheckerValid',
      value: detail.isAccountCheckerValid.value,
      expectedValue: detail.isAccountCheckerValid.expectedValue,
    });
    this.details_.push({
      name: 'IsSignedIn',
      value: detail.isSignedIn.value,
      expectedValue: detail.isSignedIn.expectedValue,
    });
    this.details_.push({
      name: 'IsSyncingBookmarks',
      value: detail.isSyncingBookmarks.value,
      expectedValue: detail.isSyncingBookmarks.expectedValue,
    });
    this.details_.push({
      name: 'IsAnonymizedUrlDataCollectionEnabled',
      value: detail.isAnonymizedUrlDataCollectionEnabled.value,
      expectedValue: detail.isAnonymizedUrlDataCollectionEnabled.expectedValue,
    });
    this.details_.push({
      name: 'IsSubjectToParentalControls',
      value: detail.isSubjectToParentalControls.value,
      expectedValue: detail.isSubjectToParentalControls.expectedValue,
    });
  }
}

customElements.define(EligibilityListElement.is, EligibilityListElement);

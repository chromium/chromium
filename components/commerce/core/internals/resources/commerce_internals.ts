// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getRequiredElement} from 'chrome://resources/js/util_ts.js';

import {CommerceInternalsApiProxy} from './commerce_internals_api_proxy.js';

function getProxy(): CommerceInternalsApiProxy {
  return CommerceInternalsApiProxy.getInstance();
}

function createLiElement(factor: string, value: boolean) {
  const li = document.createElement('li');
  li.innerText = factor + (value ? ': true' : ': false');
  return li;
}

function seeEligibleDetails() {
  getProxy().getShoppingListEligibleDetails().then(({detail}) => {
    const ul = document.createElement('ul');

    ul.appendChild(createLiElement(
        'IsRegionLockedFeatureEnabled', detail.isRegionLockedFeatureEnabled));
    ul.appendChild(createLiElement(
        'IsShoppingListAllowedForEnterprise',
        detail.isShoppingListAllowedForEnterprise));
    ul.appendChild(
        createLiElement('IsAccountCheckerValid', detail.isAccountCheckerValid));
    ul.appendChild(createLiElement('IsSignedIn', detail.isSignedIn));
    ul.appendChild(
        createLiElement('IsSyncingBookmarks', detail.isSyncingBookmarks));
    ul.appendChild(createLiElement(
        'IsAnonymizedUrlDataCollectionEnabled',
        detail.isAnonymizedUrlDataCollectionEnabled));
    ul.appendChild(createLiElement(
        'IsWebAndAppActivityEnabled', detail.isWebAndAppActivityEnabled));
    ul.appendChild(createLiElement(
        'IsSubjectToParentalControls', detail.isSubjectToParentalControls));

    document.getElementById('shopping-list-eligible-details')!.appendChild(ul);
  });
}

function initialize() {
  getRequiredElement('shopping-list-eligible-see-details-btn')
      .addEventListener('click', seeEligibleDetails);

  getProxy().getCallbackRouter().onShoppingListEligibilityChanged.addListener(
      (eligible: boolean) => {
        updateShoppingListEligibleStatus(eligible);
      });

  getProxy().getIsShoppingListEligible().then(({eligible}) => {
    updateShoppingListEligibleStatus(eligible);
  });
}

function updateShoppingListEligibleStatus(eligible: boolean) {
  const eligibleText: string = eligible ? 'true' : 'false';
  const element = getRequiredElement('shopping-list-eligible');
  element.classList.remove('eligible');
  element.classList.remove('ineligible');
  element.innerText = eligibleText;
  element.classList.add(eligible ? 'eligible' : 'ineligible');
}

document.addEventListener('DOMContentLoaded', initialize);

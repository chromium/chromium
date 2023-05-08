// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {getRequiredElement} from 'chrome://resources/js/util_ts.js';

import {EligibleEntry} from './commerce_internals.mojom-webui.js';
import {CommerceInternalsApiProxy} from './commerce_internals_api_proxy.js';

function getProxy(): CommerceInternalsApiProxy {
  return CommerceInternalsApiProxy.getInstance();
}

function entryVerificationElement(value: boolean, expected: boolean) {
  const checkmarkHTML = getTrustedHTML`&#10004;`;
  const crossmarkHTML = getTrustedHTML`&#10006;`;
  const span = document.createElement('span');
  const isValid: boolean = value === expected;
  span.innerHTML = isValid ? checkmarkHTML : crossmarkHTML;
  span.classList.add(isValid ? 'eligible' : 'ineligible');
  return span;
}

function createLiElement(factor: string, entry: EligibleEntry) {
  const li = document.createElement('li');
  li.innerText = factor + (entry.value ? ': true ' : ': false ');
  li.appendChild(entryVerificationElement(entry.value, entry.expectedValue));
  return li;
}

function seeEligibleDetails() {
  getProxy().getShoppingListEligibleDetails().then(({detail}) => {
    const element = getRequiredElement('shopping-list-eligible-details');
    getRequiredElement('shopping-list-eligible-see-details-btn').innerText =
        'Refresh';
    while (element.hasChildNodes()) {
      element.removeChild(element.firstElementChild!);
    }

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

    element.appendChild(ul);
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

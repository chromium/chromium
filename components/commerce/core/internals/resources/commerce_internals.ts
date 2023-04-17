// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CommerceInternalsApiProxy} from './commerce_internals_api_proxy.js';

function getProxy(): CommerceInternalsApiProxy {
  return CommerceInternalsApiProxy.getInstance();
}

function initialize() {
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
  document.getElementById('shopping-list-eligible')!.innerText = eligibleText;
}

document.addEventListener('DOMContentLoaded', initialize);

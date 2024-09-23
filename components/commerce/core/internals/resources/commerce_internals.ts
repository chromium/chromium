// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import type {EligibleEntry} from './commerce_internals.mojom-webui.js';
import {CommerceInternalsApiProxy} from './commerce_internals_api_proxy.js';

const SUBSCRIPTION_ROWS =
    ['Cluster ID', 'Domain', 'Price', 'Previous Price', 'Product'];
const PRODUCT_SPECIFICATIONS_ROWS =
    ['ID', 'Creation Time', 'Update Time', 'Name', 'Title', 'URLs'];
const CLUSTER_ID_COLUMN_IDX = 0;
const DOMAIN_COLUMN_IDX = 1;
const CURRENT_PRICE_COLUMN_IDX = 2;
const PREVIOUS_PRICE_COLUMN_IDX = 3;
const PRODUCT_COLUMN_IDX = 4;

const UUID_IDX = 0;
const CREATION_TIME_IDX = 1;
const UPDATE_TIME_IDX = 2;
const NAME_IDX = 3;
const TITLE_IDX = 4;
const URLS_IDX = 5;

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
        'IsSubjectToParentalControls', detail.isSubjectToParentalControls));

    element.appendChild(ul);
  });
}

function initialize() {
  getRequiredElement('shopping-list-eligible-see-details-btn')
      .addEventListener('click', seeEligibleDetails);

  getRequiredElement('reset-price-tracking-email-pref-button')
      .addEventListener('click', () => {
        getProxy().resetPriceTrackingEmailPref();
      });

  const resetMessage =
      'All your product specification sets will be removed. Are you sure?';
  getRequiredElement('reset_product_specifications_button')
      .addEventListener('click', () => {
        if (confirm(resetMessage)) {
          getProxy().resetProductSpecifications();
          location.reload();
        }
      });

  getProxy().getCallbackRouter().onShoppingListEligibilityChanged.addListener(
      (eligible: boolean) => {
        updateShoppingListEligibleStatus(eligible);
      });

  getProxy().getIsShoppingListEligible().then(({eligible}) => {
    updateShoppingListEligibleStatus(eligible);
    if (eligible) {
      renderSubscriptions();
      renderProductSpecifications();
    }
  });
}

function renderSubscriptions() {
  getProxy().getSubscriptionDetails().then(({subscriptions}) => {
    if (!subscriptions || subscriptions.length == 0) {
      return;
    }

    const subscriptionsElement = document.getElementById('subscriptions');
    if (!subscriptionsElement) {
      return;
    }
    const table = document.createElement('table');
    const thead = document.createElement('thead');
    const tr = document.createElement('tr');

    for (const colName of SUBSCRIPTION_ROWS) {
      const th = document.createElement('th');
      th.innerText = colName;
      th.setAttribute('align', 'left');
      tr.appendChild(th);
    }
    thead.appendChild(tr);
    table.appendChild(thead);

    for (let i = 0; i < subscriptions.length; i++) {
      const productInfos = subscriptions[i]!.productInfos;

      // Highlight red if there are no bookmarks for the subscription.
      const row = createRow();
      if (productInfos.length == 0) {
        row.classList.add('error-row');
        row.setAttribute('bgcolor', 'FF7F7F');
        const columns = row.getElementsByTagName('td');
        columns[CLUSTER_ID_COLUMN_IDX]!.textContent =
            BigInt(subscriptions[i]!.clusterId).toString();
        table.appendChild(row);
        continue;
      }

      for (let j = 0; j < productInfos.length; j++) {
        const columns = row.getElementsByTagName('td');
        columns[CLUSTER_ID_COLUMN_IDX]!.textContent =
            BigInt(productInfos[j]!.info!.clusterId!).toString();
        columns[DOMAIN_COLUMN_IDX]!.textContent = productInfos[j]!.info.domain!;
        columns[CURRENT_PRICE_COLUMN_IDX]!.textContent =
            productInfos[j]!.info.currentPrice!;
        columns[PREVIOUS_PRICE_COLUMN_IDX]!.textContent =
            productInfos[j]!.info.previousPrice!;

        const url = productInfos[j]!.info.productUrl.url;
        const productCell = columns[PRODUCT_COLUMN_IDX]!;
        if (url == undefined) {
          productCell.textContent = productInfos[j]!.info.title!;
        } else {
          const a = document.createElement('a');
          a.textContent = productInfos[j]!.info.title!;
          a.setAttribute('href', url);
          productCell.appendChild(a);
        }
        const imageUrl = productInfos[j]?.info.imageUrl;
        if (imageUrl != undefined) {
          const space = document.createElement('span');
          space.textContent = ' ';
          productCell.appendChild(space);
          const imgLink = document.createElement('a');
          imgLink.textContent = '(image)';
          imgLink.setAttribute('href', imageUrl.url);
          productCell.appendChild(imgLink);
        }

        row.appendChild(productCell);
        table.appendChild(row);
        subscriptionsElement.appendChild(table);
      }
    }
  });
}

function renderProductSpecifications() {
  getProxy().getProductSpecificationsDetails().then(
      ({productSpecificationsSet}) => {
        if (!productSpecificationsSet || productSpecificationsSet.length == 0) {
          return;
        }

        const productSpecificationsElement =
            document.getElementById('product_specifications');
        if (!productSpecificationsElement) {
          return;
        }
        const table = document.createElement('table');
        const thead = document.createElement('thead');
        const tr = document.createElement('tr');

        for (const colName of PRODUCT_SPECIFICATIONS_ROWS) {
          const th = document.createElement('th');
          th.innerText = colName;
          th.setAttribute('align', 'left');
          tr.appendChild(th);
        }
        thead.appendChild(tr);
        table.appendChild(thead);

        for (let i = 0; i < productSpecificationsSet.length; i++) {
          const row = createProductSpecificationsRow();
          const columns = row.getElementsByTagName('td');
          columns[UUID_IDX]!.textContent = productSpecificationsSet[i]!.uuid;
          columns[CREATION_TIME_IDX]!.textContent =
              productSpecificationsSet[i]!.creationTime;
          columns[UPDATE_TIME_IDX]!.textContent =
              productSpecificationsSet[i]!.updateTime;
          columns[NAME_IDX]!.textContent = productSpecificationsSet[i]!.name;
          for (const urlInfo of productSpecificationsSet[i]!.urlInfos) {
            const divTitle = document.createElement('div');
            divTitle.textContent = urlInfo.title;
            divTitle.classList.add('product-specs-title');
            columns[TITLE_IDX]!.appendChild(divTitle);
            const divUrl = document.createElement('div');
            divUrl.textContent = urlInfo.url.url;
            columns[URLS_IDX]!.appendChild(divUrl);
          }
          table.appendChild(row);
        }
        productSpecificationsElement.appendChild(table);
      });
}

function createProductSpecificationsRow() {
  const uuidCell = document.createElement('td');
  const creationTimeCell = document.createElement('td');
  const updateTimeCell = document.createElement('td');
  const nameCell = document.createElement('td');
  const titleCell = document.createElement('td');
  const urlsCell = document.createElement('td');
  const row = document.createElement('tr');
  for (const cell
           of [uuidCell, creationTimeCell, updateTimeCell, nameCell, titleCell,
               urlsCell]) {
    cell.vAlign = 'top';
    row.appendChild(cell);
  }
  return row;
}

function createRow() {
  const clusterIdCell = document.createElement('td');
  const domainCell = document.createElement('td');
  const currentPriceCell = document.createElement('td');
  const previousPriceCell = document.createElement('td');
  const productCell = document.createElement('td');
  const row = document.createElement('tr');
  for (const cell
           of [clusterIdCell, domainCell, currentPriceCell, previousPriceCell,
               productCell]) {
    row.appendChild(cell);
  }
  return row;
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

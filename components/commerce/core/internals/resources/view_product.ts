// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getRequiredElement} from 'chrome://resources/js/util.js';

import {CommerceInternalsApiProxy} from './commerce_internals_api_proxy.js';

function getProxy(): CommerceInternalsApiProxy {
  return CommerceInternalsApiProxy.getInstance();
}

function initialize() {
  getRequiredElement('loadProductBtn').addEventListener('click', handleSubmit);
}

function handleSubmit() {
  getProxy()
      .getProductInfoForUrl(
          {url: (getRequiredElement('productUrl') as HTMLInputElement).value})
      .then(({info}) => {
        getRequiredElement('productTitle').innerText = info.title;
        getRequiredElement('productClusterTitle').innerText = info.clusterTitle;
        getRequiredElement('productClusterId').innerText =
            BigInt.asUintN(64, info.clusterId).toString();
        getRequiredElement('productImageUrl').innerText = info.imageUrl.url;
        getRequiredElement('productCategories').innerText =
            info.categoryLabels!.join(', \n');
      });
}

document.addEventListener('DOMContentLoaded', initialize);

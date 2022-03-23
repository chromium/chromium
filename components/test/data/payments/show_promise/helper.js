/*
 * Copyright 2021 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launch PaymentRequest with a show promise that resolves with an empty
 * dictionary. The payment method to be used is 'basic-card'.
 */
function buy() { // eslint-disable-line no-unused-vars
    buyWithMethods('basic-card');
}

/**
 * Launch PaymentRequest with a show promise that resolves with an empty
 * dictionary. The payment method to be used is the current url of the page.
 * @return {string} - The error message, if any.
 */
async function buyWithCurrentUrlMethod() { // eslint-disable-line no-unused-vars
    return buyWithMethods(window.location.href);
}

/**
 * Launch PaymentRequest with a show promise that resolves with an empty
 * dictionary. The payment method to be used is 'https://bobpay.com'.
 */
function buyWithUrlMethod() { // eslint-disable-line no-unused-vars
    buyWithMethods('https://bobpay.com');
}

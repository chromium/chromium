/*
 * Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

var timeoutID1;
var timeoutID2;

/**
 * Prints the given error message.
 * @param {string} msg - The error message to print.
 */
function error(msg) {
  if (timeoutID1) {
    window.clearTimeout(timeoutID1);
  }
  if (timeoutID2) {
    window.clearTimeout(timeoutID2);
  }
  let element = document.createElement('pre');
  element.innerHTML = msg;
  element.className = 'error';
  document.getElementById('msg').appendChild(element);
  timeoutID1 = window.setTimeout(function() {
    if (element.className !== 'error') {
      return;
    }
    element.className = 'error-hide';
    timeoutID2 = window.setTimeout(function() {
      element.innerHTML = '';
      element.className = '';
    }, 500);
  }, 10000);
}

/**
 * Prints the given informational message.
 * @param {string} msg - The information message to print.
 */
function info(msg) {
  let element = document.createElement('pre');
  element.innerHTML = msg;
  element.className = 'info';
  document.getElementById('msg').appendChild(element);
}

/**
 * Converts an address object into a dictionary.
 * @param {PaymentAddress} addr - The address to convert.
 * @return {object} The resulting dictionary.
 */
function toDictionary(addr) {
  let dict = {};
  if (addr) {
    if (addr.toJSON) {
      return addr;
    }
    dict.country = addr.country;
    dict.region = addr.region;
    dict.city = addr.city;
    dict.dependentLocality = addr.dependentLocality;
    dict.addressLine = addr.addressLine;
    dict.postalCode = addr.postalCode;
    dict.sortingCode = addr.sortingCode;
    dict.languageCode = addr.languageCode;
    dict.organization = addr.organization;
    dict.recipient = addr.recipient;
    dict.phone = addr.phone;
  }
  return dict;
}

/**
 * Called when the payment request is complete.
 * @param {string} message - The human readable message to display.
 * @param {PaymentResponse} resp - The payment response.
 */
function done(message, resp) {
  let element = document.getElementById('contents');
  element.innerHTML = message;

  if (resp.toJSON) {
    info(JSON.stringify(resp, undefined, 2));
    return;
  }

  let shippingOption = resp.shippingOption ?
      'shipping, delivery, pickup option: ' + resp.shippingOption + '<br/>' :
      '';

  let shippingAddress = resp.shippingAddress ?
      'shipping, delivery, pickup address: ' +
          JSON.stringify(toDictionary(resp.shippingAddress), undefined, 2) +
          '<br/>' :
      '';

  let instrument =
      'instrument:' + JSON.stringify(resp.details, undefined, 2) + '<br/>';

  let method = 'method: ' + resp.methodName + '<br/>';
  let email = resp.payerEmail ? 'email: ' + resp.payerEmail + '<br/>' : '';
  let phone = resp.payerPhone ? 'phone: ' + resp.payerPhone + '<br/>' : '';
  let name = resp.payerName ? 'name: ' + resp.payerName + '<br/>' : '';


  info(
      email + phone + name + shippingOption + shippingAddress + method +
      instrument);
}

/**
 * Clears all messages.
 */
function clearAllMessages() {
  document.getElementById('msg').innerHTML = '';
}

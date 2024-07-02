// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';
// </if>

import 'chrome://resources/js/jstemplate_compiled.js';
import './strings.m.js';

import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';
import {$} from 'chrome://resources/js/util.js';

// TODO(vishwath): This function is identical to the one in sync_internals.js
// Merge both if possible.
// Accepts a DOM node and sets its highlighted attribute oldVal !== newVal
function highlightIfChanged(node, oldVal, newVal) {
  const oldStr = oldVal.toString();
  const newStr = newVal.toString();
  if (oldStr !== '' && oldStr !== newStr) {
    // Note the addListener function does not end up creating duplicate
    // listeners.  There can be only one listener per event at a time.
    // Reference: https://developer.mozilla.org/en/DOM/element.addEventListener
    node.addEventListener('webkitAnimationEnd', function() {
      this.removeAttribute('highlighted');
    }, false);
    node.setAttribute('highlighted', '');
  }
}

// Wraps highlightIfChanged for multiple conditions.
function highlightIfAnyChanged(node, oldToNewValList) {
  for (let i = 0; i < oldToNewValList.length; i++) {
    highlightIfChanged(node, oldToNewValList[i][0], oldToNewValList[i][1]);
  }
}

function setClassFromValue(value) {
  if (value === 0) {
    return 'zero';
  }
  if (value === 'Successful') {
    return 'ok';
  }

  return '';
}

// Set on window for jstemplate.
window.highlightIfChanged = highlightIfChanged;
window.highlightIfAnyChanged = highlightIfAnyChanged;
window.setClassFromValue = setClassFromValue;

let internalsInfo = {};

// Replace the displayed values with the latest fetched ones.
function refreshSigninInfo(signinInfo) {
  // Process templates even against an empty `signinInfo` to hide some sections.
  internalsInfo = signinInfo;
  jstProcess(new JsEvalContext(signinInfo), $('signin-info'));
  jstProcess(new JsEvalContext(signinInfo), $('token-info'));
  jstProcess(new JsEvalContext(signinInfo), $('account-info'));
  jstProcess(new JsEvalContext(signinInfo), $('refresh-token-events'));
  jstProcess(new JsEvalContext(signinInfo), $('bound-session-info'));
  document
      .querySelectorAll(
          'td[jsvalues=".textContent: status"], td[jscontent="expirationTime"]')
      .forEach(td => {
        if (td.textContent.includes('Expired at')) {
          td.style = 'color: #ffffff; background-color: #ff0000';
        }
      });
}

// Replace the cookie information with the fetched values.
function updateCookieAccounts(cookieAccountsInfo) {
  jstProcess(new JsEvalContext(cookieAccountsInfo), $('cookie-info'));
}

// On load, do an initial refresh and register refreshSigninInfo to be invoked
// whenever we get new signin information from SigninInternalsUI.
function onLoad() {
  addWebUiListener('signin-info-changed', refreshSigninInfo);
  addWebUiListener('update-cookie-accounts', updateCookieAccounts);

  sendWithPromise('getSigninInfo').then(refreshSigninInfo);
}

document.addEventListener('DOMContentLoaded', onLoad, false);

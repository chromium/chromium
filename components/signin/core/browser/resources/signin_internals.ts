// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';
// </if>

import 'chrome://resources/js/jstemplate_compiled.js';
import '/strings.m.js';

import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

interface CookieAccountsInfo {}
interface SigninInfo {}

function setClassFromValue(value: string): string {
  if (value === 'Successful') {
    return 'ok';
  }

  return '';
}

// Set on window for jstemplate.
Object.assign(window, {setClassFromValue});

// Replace the displayed values with the latest fetched ones.
function refreshSigninInfo(signinInfo: SigninInfo) {
  // Process templates even against an empty `signinInfo` to hide some sections.
  jstProcess(new JsEvalContext(signinInfo), getRequiredElement('signin-info'));
  jstProcess(new JsEvalContext(signinInfo), getRequiredElement('token-info'));
  jstProcess(new JsEvalContext(signinInfo), getRequiredElement('account-info'));
  jstProcess(
      new JsEvalContext(signinInfo),
      getRequiredElement('refresh-token-events'));
  jstProcess(
      new JsEvalContext(signinInfo), getRequiredElement('bound-session-info'));
  document
      .querySelectorAll(
          'td[jsvalues=".textContent: status"], td[jscontent="expirationTime"]')
      .forEach(td => {
        if (td.textContent!.includes('Expired at')) {
          td.setAttribute('style', 'color: #ffffff; background-color: #ff0000');
        }
      });
}

// Replace the cookie information with the fetched values.
function updateCookieAccounts(cookieAccountsInfo: CookieAccountsInfo) {
  jstProcess(
      new JsEvalContext(cookieAccountsInfo), getRequiredElement('cookie-info'));
}

// On load, do an initial refresh and register refreshSigninInfo to be invoked
// whenever we get new signin information from SigninInternalsUI.
function onLoad() {
  addWebUiListener('signin-info-changed', refreshSigninInfo);
  addWebUiListener('update-cookie-accounts', updateCookieAccounts);

  sendWithPromise('getSigninInfo').then(refreshSigninInfo);
}

document.addEventListener('DOMContentLoaded', onLoad);

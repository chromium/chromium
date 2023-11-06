// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {HIDDEN_CLASS} from 'chrome://interstitials/common/resources/interstitial_common.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

/**
 * This appends a piece of debugging information to the end of the warning.
 * When complete, the caller must also make the debugging div
 * (error-debugging-info) visible.
 * @param {string} title  The name of this debugging field.
 * @param {string} value  The value of the debugging field.
 * @param {boolean=} fixedWidth If true, the value field is displayed fixed
 *                              width.
 */
function appendDebuggingField(title, value, fixedWidth) {
  // The values input here are not trusted. Never use innerHTML on these
  // values!
  const spanTitle = document.createElement('span');
  spanTitle.classList.add('debugging-title');
  spanTitle.innerText = title + ': ';

  const spanValue = document.createElement('span');
  spanValue.classList.add('debugging-content');
  if (fixedWidth) {
    spanValue.classList.add('debugging-content-fixed-width');
  }
  spanValue.innerText = value;

  const pElem = document.createElement('p');
  pElem.classList.add('debugging-content');
  pElem.appendChild(spanTitle);
  pElem.appendChild(spanValue);
  document.querySelector('#error-debugging-info').appendChild(pElem);
}

function toggleDebuggingInfo() {
  const hiddenDebug = document.querySelector('#error-debugging-info')
                          .classList.toggle(HIDDEN_CLASS);
  document.querySelector('#error-code')
      .setAttribute('aria-expanded', !hiddenDebug);
}

export function setupSSLDebuggingInfo() {
  if (loadTimeData.getString('type') !== 'SSL') {
    return;
  }

  // The titles are not internationalized because this is debugging information
  // for bug reports, help center posts, etc.
  appendDebuggingField('Subject', loadTimeData.getString('subject'));
  appendDebuggingField('Issuer', loadTimeData.getString('issuer'));
  appendDebuggingField('Expires on', loadTimeData.getString('expirationDate'));
  appendDebuggingField('Current date', loadTimeData.getString('currentDate'));
  appendDebuggingField('PEM encoded chain', loadTimeData.getString('pem'),
                       true);
  const ctInfo = loadTimeData.getString('ct');
  if (ctInfo) {
    appendDebuggingField('Certificate Transparency', ctInfo);
  }

  const errorCode = document.querySelector('#error-code');
  errorCode.addEventListener('click', toggleDebuggingInfo);
  errorCode.setAttribute('role', 'button');
  errorCode.setAttribute('aria-expanded', false);
}

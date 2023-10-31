// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for user_actions.html, served from chrome://user-actions/
 * This is used to debug user actions recording. It displays a live
 * stream of all user action events that occur in chromium while the
 * chrome://user-actions/ page is open.
 *
 * The simple object defined in this javascript file listens for
 * callbacks from the C++ code saying that a new user action was seen.
 */

import {addWebUiListener} from 'chrome://resources/js/cr.js';
import {$} from 'chrome://resources/js/util.js';

/**
 * Appends a row to the output table listing the user action observed
 * and the current timestamp.
 * @param {string} userAction the name of the user action observed.
 */
function observeUserAction(userAction) {
  const table = $('user-actions-table');
  const tr = document.createElement('tr');
  let td = document.createElement('td');
  td.textContent = userAction;
  tr.appendChild(td);
  td = document.createElement('td');
  td.textContent = Date.now() / 1000;  // in seconds since epoch
  tr.appendChild(td);
  table.appendChild(tr);
}

document.addEventListener('DOMContentLoaded', function() {
  addWebUiListener('user-action', observeUserAction);
  // <if expr="not is_ios">
  chrome.send('pageLoaded');
  // </if>
});

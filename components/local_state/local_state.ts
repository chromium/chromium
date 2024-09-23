// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for local_state.html, served from chrome://local-state/
 * This is used to debug the contents of the Local State file.
 */

// <if expr="is_ios">
// This is needed for the iOS implementation of chrome.send (to communicate
// between JS and native).
// TODO(crbug.com/41173939): Remove this once injected by web.
import 'chrome://resources/js/ios/web_ui.js';

// </if>

import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

// When the page loads, request the JSON local state data from C++.
document.addEventListener('DOMContentLoaded', function() {
  sendWithPromise('requestJson').then((localState: string) => {
    getRequiredElement('content').textContent = localState;
  });
});

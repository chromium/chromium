// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util_ts.js';

document.addEventListener('DOMContentLoaded', function() {
  sendWithPromise('fetchClientId').then((clientId: string) => {
    getRequiredElement('content').textContent = clientId;
  });
});

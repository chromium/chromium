// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.js';

document.addEventListener('DOMContentLoaded', function() {
  sendWithPromise('fetchClientId').then((clientId: string) => {
    $('content').textContent = clientId;
  });
});

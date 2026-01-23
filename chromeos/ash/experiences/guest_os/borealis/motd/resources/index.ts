// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from './browser_proxy.js';

function dismiss() {
  BrowserProxy.getInstance().handler.onDismiss();
}

function initialize() {
  const btn = document.getElementById('btn')!;
  btn.addEventListener('click', dismiss);
  let timeoutId = setTimeout(function() {
    dismiss();
  }, 200);
  addEventListener('message', function() {
    clearTimeout(timeoutId);
    const iframe = document.getElementById('motd')!;
    iframe.hidden = false;
    const placeholder = document.getElementById('placeholder')!;
    placeholder.hidden = true;
  });
}

document.addEventListener('DOMContentLoaded', initialize);

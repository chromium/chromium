// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from './browser_proxy.js';

function dismiss() {
  BrowserProxy.getInstance().handler.onDismiss();
}

function uninstall() {
  BrowserProxy.getInstance().handler.onUninstall();
}

function initialize() {
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

  const btn = document.getElementById('btn')!;
  btn.addEventListener('click', dismiss);

  const uninstallBtn = document.getElementById('uninstall-btn')!;
  uninstallBtn.addEventListener('click', uninstall);

  BrowserProxy.getInstance().handler.isBorealisInstalled().then(
    (result: {isInstalled: boolean}) => {
    uninstallBtn.hidden = !result.isInstalled;
  });
}

document.addEventListener('DOMContentLoaded', initialize);

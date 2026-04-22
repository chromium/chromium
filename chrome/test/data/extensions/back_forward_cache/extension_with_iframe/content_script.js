// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const iframe = document.createElement('iframe');
iframe.src = chrome.runtime.getURL('iframe.html');
document.body.appendChild(iframe);
chrome.test.sendMessage('iframe_injected');

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

document.title = 'modified';

var port = chrome.runtime.connect();

window.addEventListener('pagehide', () => {
  port.postMessage('pagehide');
});

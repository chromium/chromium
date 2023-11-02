// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  let element = document.getElementById('main');
  let style = getComputedStyle(element);
  let color = style.getPropertyValue('color');
  sendResponse(color);
});

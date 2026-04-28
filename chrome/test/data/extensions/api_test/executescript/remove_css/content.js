// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  const element = document.getElementById('main');
  const style = getComputedStyle(element);
  const color = style.getPropertyValue('color');
  sendResponse(color);
});

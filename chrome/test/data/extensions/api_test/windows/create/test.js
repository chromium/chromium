// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function typeNormal() {
    chrome.windows.create({'type': 'normal'}, chrome.test.callbackPass(w => {
      chrome.test.assertEq('normal', w.type);
    }));
  },
  function typePopup() {
    chrome.windows.create({'type': 'popup'}, chrome.test.callbackPass(w => {
      chrome.test.assertEq('popup', w.type);
    }));
  },
]);

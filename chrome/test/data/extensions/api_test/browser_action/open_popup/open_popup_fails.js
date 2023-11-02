// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Notify pass to return control to the ApiTest. This needs to be done in 2
// passes so we can open the popup from the ApiTest side because loading an
// extension hides any open popups. After a popup is opened, we can assert that
// a popup fails to open with this API.
chrome.test.notifyPass();
chrome.test.sendMessage('ready', function(reply) {
  if (reply !== 'show another')
    return;
  chrome.browserAction.openPopup(function(popupWindow2) {
    // This popup should fail to open.
    chrome.test.assertTrue(!popupWindow2);
    chrome.test.notifyPass();
  });
});

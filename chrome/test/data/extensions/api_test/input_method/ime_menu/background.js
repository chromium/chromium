// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.inputMethodPrivate.getInputMethodConfig(function(config) {
  chrome.test.assertTrue(config["isImeMenuActivated"]);
  chrome.test.sendMessage('config_ready');
});

// We just add the listener without receiving any event at first time. The test
// continues in input_method_apitest_chromeos.cc to trigger the event, and
// succeeds after getting the 'event_ready' message.
chrome.inputMethodPrivate.onImeMenuActivationChanged.addListener(
  function(isActive) {
    chrome.test.assertFalse(isActive);
    // Wait for the 'event_ready' message in
    // ExtensionInputMethodApiTest.ImeMenuActivation.
    chrome.test.sendMessage('event_ready');
});

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Sends a message when InstallLanguageRequest event is received. The
// tts_extension_apitest.cc test verifies the params were sent successfully
chrome.test.runTests([function testOnInstallLanguageRequestEventEmitted() {
  var listener = (requestor, lang) => {
    chrome.test.sendMessage(`lang:${lang}`);
    chrome.test.sendMessage(
        `requestor.id:${requestor.id}, requestor.source:${requestor.source}`);
  };

  chrome.ttsEngine.onInstallLanguageRequest.addListener(listener);
}]);

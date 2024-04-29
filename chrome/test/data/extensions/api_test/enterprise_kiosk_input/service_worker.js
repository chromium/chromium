// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessageExternal.addListener(
  ({ message, data }, _sender, sendResponse) => {
    switch (message) {
      case "is_api_available":
        runIsApiAvailable(sendResponse);
        return true;
      case "set_input_method":
        runSetInputMethod(data, sendResponse);
        return true;
      case "version":
        sendResponse("1.3");
        return false;
    }
    sendResponse('unknown message "' + message + '"');
    return false;
  }
);

function runIsApiAvailable(sendResponse) {
  sendResponse(chrome.enterprise?.kioskInput != null);
}

function runSetInputMethod(data, sendResponse) {
  chrome.enterprise.kioskInput.setCurrentInputMethod(
    { inputMethodId: data },
    () => sendResponse(chrome.runtime.lastError?.message ?? true)
  );
}

// Trigger `kReadyMessageReceived` in `extension_force_install_mixin.cc`.
chrome.test?.sendMessage && chrome.test.sendMessage('ready')

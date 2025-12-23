// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Set the current input method so this extension will receive IME events.
chrome.inputMethodPrivate.setCurrentInputMethod(
    '_ext_ime_ilanclmaeigfpnmdlgelmhkpkegdioiptest', () => {
      chrome.test.sendMessage('set_input_success');
    });

chrome.input.ime.onKeyEvent.addListener(function(unusedEngineID, keyData) {
  if (keyData.type == 'keydown') {
    chrome.test.sendMessage(keyData.key);
  }

  // Consume the event to prevent browser handling (e.g. navigation).
  return true;
});

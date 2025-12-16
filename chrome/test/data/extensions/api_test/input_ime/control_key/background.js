// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Set the current input method so this extension will receive IME events.
chrome.inputMethodPrivate.setCurrentInputMethod(
    '_ext_ime_ilanclmaeigfpnmdlgelmhkpkegdioiptest', () => {
      chrome.test.sendMessage('set_input_success');
    });

chrome.input.ime.onKeyEvent.addListener(function(unusedEngineID, keyData) {
  // TODO(crbug.com/467185174): 'Ctrl' should be 'Control".
  if (keyData.type == 'keydown' && keyData.key === 'Ctrl') {
    chrome.test.succeed();
  }

  // The test doesn't need to actually modify the value so let it pass through
  // to the DOM as-is.
  return false;
});

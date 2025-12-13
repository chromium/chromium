// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var context_id = -1;

// Set the current input method so this extension will receive IME events.
chrome.inputMethodPrivate.setCurrentInputMethod(
    '_ext_ime_ilanclmaeigfpnmdlgelmhkpkegdioiptest', () => {
      chrome.test.sendMessage('set_input_success');
    });

// Reset context_id on blur since it is no longer valid.
chrome.input.ime.onBlur.addListener(function(contextID) {
  if (context_id === contextID) {
    context_id = -1;
  }
});

// Record context id of focused text box so we can change the text.
chrome.input.ime.onFocus.addListener(function(context) {
  context_id = context.contextID;
});

// Change all lowercase text to uppercase.
chrome.input.ime.onKeyEvent.addListener(function(unusedEngineID, keyData) {
  if (keyData.type == 'keydown' && keyData.key.match(/^[a-z]$/)) {
    chrome.input.ime.commitText(
        {'contextID': context_id, 'text': keyData.key.toUpperCase()});
    // Delay this message to allow time for the browser to update the UI with
    // the new uppercase text.
    setTimeout(() => {
      chrome.test.sendMessage('ime_listener_finished');
    }, 1);
    return true;
  }
  return false;
});

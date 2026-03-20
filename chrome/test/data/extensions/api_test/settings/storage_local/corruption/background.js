// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Universal background script supporting ExtensionCorruptLocalSettingsApiTest.
 *
 * Implements a command loop to execute arbitrary storage operations
 * sequentially.
 */

// Audit changes (needed for
// `ExtensionCorruptLocalSettingsApiTest.InvalidJsonClear` verification).
chrome.storage.onChanged.addListener((changes, namespace) => {
  if (namespace === 'local') {
    chrome.test.sendMessage(`on_changed_success: ${JSON.stringify(changes)}`);
  }
});

function onCommand(command) {
  if (command === 'get_keys') {
    chrome.storage.local.get(['test_key', 'good_key'], (data) => {
      report('get', data);
    });
  } else if (command === 'get_all') {
    chrome.storage.local.get(null, (data) => {
      report('get_all', data);
    });
  } else if (command === 'clear') {
    chrome.storage.local.clear(() => {
      report('clear');
    });
  } else if (command === 'get_test_key') {
    chrome.storage.local.get(['test_key'], (data) => {
      report('get_test_key', data);
    });
  } else if (command === 'get_good_key') {
    chrome.storage.local.get(['good_key'], (data) => {
      report('get_good_key', data);
    });
  } else if (command === 'set_data') {
    chrome.storage.local.set(
        {test_key: 'test_value', good_key: 'good_value'}, () => {
          report('set');
        });
  }
}

/**
 * Reports the results of a storage operation back to C++ and chains triggers.
 *
 * @param {string} operation Name of operation (e.g., 'get', 'clear').
 * @param {Object=} data Optional response payload that was read.
 *
 * NOTE: If the C++ `reply` argument is non-empty, it is interpreted as the next
 * command string and automatically executes on the command loop. Empty replies
 * terminate the command chain cycle.
 */
function report(operation, data) {
  let message = operation;
  if (chrome.runtime.lastError) {
    message += `_error: ${chrome.runtime.lastError.message}`;
  } else {
    message += `_success${data ? `: ${JSON.stringify(data)}` : ''}`;
  }

  chrome.test.sendMessage(message, (reply) => {
    if (reply) {
      onCommand(reply);
    }
  });
}

chrome.test.sendMessage('background_ready', onCommand);

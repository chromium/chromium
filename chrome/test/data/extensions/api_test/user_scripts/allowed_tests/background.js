// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kMatches = ['http://example.com/*'];
const kRunAt = 'document_end';

const kUserScriptCode = `var div = document.createElement('div');
     div.id = 'user-script-code';
     document.body.appendChild(div);`

async function verifyApiIsNotAvailable() {
  let message;
  try {
    await chrome.userScripts.getScripts();
    message = 'failure (chrome.userScripts API is available)';
  } catch (e) {
    const expectedError =
        'TypeError: Cannot read properties of undefined (reading ' +
        '\'getScripts\')';
    message = e.toString() == expectedError ?
        'success' :
        'Unexpected error: ' + e.toString();
  }
  chrome.test.sendScriptResult(message);
}

async function registerUserScripts() {
  const userScripts = [
    {
      id: 'user_script-file',
      matches: kMatches,
      js: [{file: 'user_script.js'}],
      runAt: kRunAt,
    },
    {
      id: 'user_script_code',
      matches: kMatches,
      js: [{code: kUserScriptCode}],
      runAt: kRunAt,
    }
  ];
  await chrome.userScripts.register(userScripts);
  chrome.test.sendScriptResult('success');
}

async function registerContentScript() {
  const contentScript = {
    id: 'content_script',
    matches: kMatches,
    js: ['content_script.js'],
    runAt: kRunAt,
  };
  await chrome.scripting.registerContentScripts([contentScript]);
  chrome.test.sendScriptResult('success');
}

// Signal that the worker has started to the test when first installed and when
// browser restarts.
function sendWorkerStartedMessage() {
  chrome.test.sendMessage('started');
}
chrome.runtime.onInstalled.addListener(sendWorkerStartedMessage);
chrome.runtime.onStartup.addListener(sendWorkerStartedMessage);

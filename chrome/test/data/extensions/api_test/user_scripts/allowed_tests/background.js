// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kMatches = ['http://example.com/*'];
const kRunAt = 'document_end';

const kUserScriptCode = `var div = document.createElement('div');
     div.id = 'user-script-code';
     document.body.appendChild(div);`

async function checkApiAvailability() {
  // API hasn't been bound in this worker session before.
  if (chrome.userScripts === undefined) {
    chrome.test.sendScriptResult('unavailable');
    return;
  }

  // If the API was bound before in this same worker session then it can still
  // look bound so let's call an API method to see if it's actually still
  // available to use.
  var apiAvailableStatus;
  try {
    await chrome.userScripts.getScripts();
    apiAvailableStatus = 'available';
  } catch (e) {
    apiAvailableStatus = 'unavailable';
  }
  chrome.test.sendScriptResult(apiAvailableStatus);
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

  try {
    await chrome.userScripts.register(userScripts);
    chrome.test.sendScriptResult('success');
  } catch (e) {
    chrome.test.sendScriptResult(
        'chrome.userScripts.register() threw error:' + error);
  }
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

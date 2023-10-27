// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kMatches = ['http://example.com/*'];
const kRunAt = 'document_end';

const kUserScriptCode =
    `var div = document.createElement('div');
     div.id = 'user-script-code';
     document.body.appendChild(div);`

async function registerUserScripts() {
  const userScripts =
      [
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
  const contentScript =
      {
        id: 'content_script',
        matches: kMatches,
        js: ['content_script.js'],
        runAt: kRunAt,
      };
  await chrome.scripting.registerContentScripts([contentScript]);
  chrome.test.sendScriptResult('success');
}

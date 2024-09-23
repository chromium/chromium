// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

async function openExampleUrl() {
  const port = (await chrome.test.getConfig()).testServer.port;
  const url = `http://example.com:${port}/simple.html`;
  return await openTab(url);
}

// Individual code pieces that append to a document's title.
const codeA = {code: `document.title += ' script aaaa';`};
const codeB = {code: `document.title += ' script bbbb';`};
const codeC = {code: `document.title += ' script cccc';`};

// Individual scripts using the code blocks.
const scriptA =
    {
      id: 'aaaa',
      matches: ['*://example.com/*'],
      js: [codeA],
      runAt: 'document_end',
    };
const scriptB =
    {
      id: 'bbbb',
      matches: ['*://example.com/*'],
      js: [codeB],
      runAt: 'document_end',
    };
const scriptC =
    {
      id: 'cccc',
      matches: ['*://example.com/*'],
      js: [codeC],
      runAt: 'document_end',
    };

// A series of tests to exercise injection order of user scripts relative to
// one another (within the same extension).
// TODO(crbug.com/337078958): These are inconsistent with one another and should
// probably be updated.
chrome.test.runTests([
  // Tests that user scripts from separate registrations are executed in the
  // order of those registrations. That is, a script registered first will
  // inject before a script registered later.
  async function userScriptsInjectInRegistrationOrderBetweenRegistrations() {
    // Register B, then A, then C.
    await chrome.userScripts.register([scriptB]);
    await chrome.userScripts.register([scriptA]);
    await chrome.userScripts.register([scriptC]);

    const tab = await openExampleUrl();

    // Injection order should have been the same as registration order, which
    // is scriptB, scriptA, scriptC.
    chrome.test.assertEq('OK script bbbb script aaaa script cccc', tab.title);

    await chrome.userScripts.unregister();

    chrome.test.succeed();
  },

  // Tests that, when registering multiple scripts at once, scripts are injected
  // based on alphabetical order by ID.
  async function userScriptsInjectInAlphabeticalIdOrderWithinOneRegistration() {
    // Register B, A, and C all in the same registration call.
    await chrome.userScripts.register([scriptB, scriptA, scriptC]);

    const tab = await openExampleUrl();

    // Injection order is the alphabetical based on ID, which is then scriptA,
    // scriptB, scriptC.
    chrome.test.assertEq('OK script aaaa script bbbb script cccc', tab.title);

    await chrome.userScripts.unregister();

    chrome.test.succeed();
  },

  // Tests that, when multiple pieces of code are added in a single user script
  // registration, the injection order is the same as the order the code pieces
  // are declared in.
  async function individualScriptsInAUserScriptInjectInDeclaredOrder() {
    const sharedScript =
        {
          id: 'cccc',
          matches: ['*://example.com/*'],
          js: [codeB, codeA, codeC],
          runAt: 'document_end',
        };
    await chrome.userScripts.register([sharedScript]);

    // Register a single user script with all three code pieces.
    const tab = await openExampleUrl();

    // Injection order is the same as the declared order in the script, which
    // is B, A, C.
    chrome.test.assertEq('OK script bbbb script aaaa script cccc', tab.title);

    await chrome.userScripts.unregister();

    chrome.test.succeed();

  },
]);

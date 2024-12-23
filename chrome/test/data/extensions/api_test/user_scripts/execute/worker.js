// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  // Test that an error is returned if the user script source is not specified.
  async function invalidScriptSource_EmptyJs() {
    await chrome.userScripts.unregister();

    const script = {js: {}, target: {tabId: 123}};
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(script),
        `Error: User script must specify exactly one of 'code' or 'file' as ` +
            `a js source.`);

    chrome.test.succeed();
  },

  // Test that an error is returned if the user script source specifies both
  // code and file.
  async function invalidScriptSource_MultipleSources() {
    await chrome.userScripts.unregister();

    const script = {js: {file: 'script.js', code: ''}, target: {tabId: 123}};
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(script),
        `Error: User script must specify exactly one of 'code' or 'file' as ` +
            `a js source.`);

    chrome.test.succeed();
  },

  // Tests that an error is returned if the user scrip specifies injection to
  // all frames and also a specific set of frame ids.
  async function invalidAllFrames() {
    await chrome.userScripts.unregister();

    const script = {
      js: {file: 'script.js'},
      target: {allFrames: true, frameIds: [456], tabId: 123}
    };
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(script),
        `Error: User script must not specify injection to 'all frames' when ` +
            `it has a specific set of 'frameIds' to inject into.`);

    chrome.test.succeed();
  },

  // Test that an error is returned is the user script has both document ids and
  // frame ids as injection targets.
  async function invalidTargetIds() {
    await chrome.userScripts.unregister();

    const script = {
      js: {file: 'script.js'},
      target: {documentIds: ['documentId'], frameIds: [456], tabId: 123}
    };
    await chrome.test.assertPromiseRejects(
        chrome.userScripts.execute(script),
        `Error: User script must not specify both 'documentIds' and ` +
            `'frameIds'.`);

    chrome.test.succeed();
  }
])

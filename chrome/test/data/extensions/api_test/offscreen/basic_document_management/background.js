// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const VALID_PARAMS =
    {
      url: 'offscreen.html',
      reasons: ['TESTING'],
      justification: 'ignored',
    };

async function hasOffscreenDocument() {
  const contexts =
      await chrome.runtime.getContexts(
          {contextTypes: [chrome.runtime.ContextType.OFFSCREEN_DOCUMENT]});
  chrome.test.assertTrue(!!contexts);
  chrome.test.assertTrue(contexts.length <= 1);
  return contexts.length == 1;
}

chrome.test.runTests([
  async function createDocumentAndEnsureItExistsAndThenClose() {
    chrome.test.assertFalse(await hasOffscreenDocument());

    await chrome.offscreen.createDocument(VALID_PARAMS);
    chrome.test.assertTrue(await hasOffscreenDocument());

    // Sanity check that the document exists and can be reached by passing a
    // message and expecting a reply. Note that general offscreen document
    // behavior is tested more in the OffscreenDocumentHost tests, so this is
    // mostly just ensuring that it works when the document is created from the
    // API.
    const reply = await chrome.runtime.sendMessage('message from background');
    chrome.test.assertEq(
        {
          msg: 'message from background',
          reply: 'offscreen reply',
        },
        reply);

    // Close the document to tidy up for the next test.
    await chrome.offscreen.closeDocument();
    chrome.test.assertFalse(await hasOffscreenDocument());

    chrome.test.succeed();
  },

  async function createDocumentWithAbsoluteSameOriginUrlSucceeds() {
    chrome.test.assertFalse(await hasOffscreenDocument());

    await chrome.offscreen.createDocument(
        {
          url: chrome.runtime.getURL('offscreen.html'),
          reasons: ['TESTING'],
          justification: 'ignored',
        });
    chrome.test.assertTrue(await hasOffscreenDocument());

    // Tidy up.
    await chrome.offscreen.closeDocument();
    chrome.test.succeed();
  },

  async function createDocumentWithInvalidUrlsRejects() {
    chrome.test.assertFalse(await hasOffscreenDocument());

    const urlsToTest = [
      // Web URL
      'https://example.com',
      // Another extension's resource
      `chrome-extension://${'a'.repeat(32)}/offscreen.html`,
      // Invalid URL. It's, funnily, not enough to just have "an invalid url",
      // because that would be treated as a relative path and appended to the
      // extension origin. Force it to be significantly less valid.
      'http://:/<>/',
    ];

    for (let url of urlsToTest) {
      await chrome.test.assertPromiseRejects(
          chrome.offscreen.createDocument(
              {
                url,
                reasons: ['TESTING'],
                justification: 'ignored',
              }),
          'Error: Invalid URL.');
      // No document should have been created.
      chrome.test.assertFalse(await hasOffscreenDocument());
    }
    chrome.test.succeed();
  },

  async function cannotCreateMoreThanOneOffscreenDocument() {
    chrome.test.assertFalse(await hasOffscreenDocument());
    await chrome.offscreen.createDocument(VALID_PARAMS);
    chrome.test.assertTrue(await hasOffscreenDocument());

    await chrome.test.assertPromiseRejects(
        chrome.offscreen.createDocument(VALID_PARAMS),
        'Error: Only a single offscreen document may be created.');

    // Tidy up.
    await chrome.offscreen.closeDocument();
    chrome.test.succeed();
  },

  async function callingCloseDocumentWhenNoneOpenRejects() {
    chrome.test.assertFalse(await hasOffscreenDocument());
    await chrome.test.assertPromiseRejects(
        chrome.offscreen.closeDocument(),
        'Error: No current offscreen document.');
    chrome.test.succeed();
  },

  async function callingCreateDocumentWithNoReasonsRejects() {
    await chrome.test.assertPromiseRejects(
        chrome.offscreen.createDocument(
        {
          url: 'offscreen.html',
          reasons: [],
          justification: 'ignored',
        }),
        'Error: A `reason` must be provided.');
    chrome.test.succeed();
  },

  async function callingCreateDocumentWithMultipleReasonsIsAccepted() {
    chrome.test.assertFalse(await chrome.offscreen.hasDocument());
    await chrome.offscreen.createDocument(
        {
          url: 'offscreen.html',
          reasons: ['TESTING', 'AUDIO_PLAYBACK'],
          justification: 'ignored',
        });
    chrome.test.assertTrue(await chrome.offscreen.hasDocument());

    // Tidy up.
    await chrome.offscreen.closeDocument();
    chrome.test.succeed();
  },

  async function nonexistentRelativePathIsAccepted() {
    chrome.test.assertFalse(await hasOffscreenDocument());
    // Questionable behavior: A non-existent relative path is accepted and the
    // document is created. In many cases, this could be an extension bug, but
    // there are valid cases when extensions may do this (e.g., with custom
    // service worker handling for a fetch event). Additionally, this matches
    // our behavior with other contexts like tabs (where we will commit the URL,
    // but show an error).
    await chrome.offscreen.createDocument(
        {
          url: 'non_extistent.html',
          reasons: ['TESTING'],
          justification: 'ignored',
        });

    // However, the document is *not* associated with the extension (because it
    // commits to an error page). Because of this, `runtime.getContexts()` does
    // not return the context.
    // TODO(devlin): That leads to a bit of a problem where an extension has an
    // offscreen document (and thus trying to create a new one won't succeed),
    // but the result isn't included in runtime.getContexts(), implying there
    // isn't one. But, `offscreen.closeDocument()` would still work. Is that a
    // big enough thorn that we need to fix it?
    chrome.test.assertFalse(await hasOffscreenDocument());

    // Tidy up.
    await chrome.offscreen.closeDocument();
    chrome.test.succeed();
  },
])

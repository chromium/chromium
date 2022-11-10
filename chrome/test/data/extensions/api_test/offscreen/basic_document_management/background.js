// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const VALID_PARAMS =
    {
      url: 'offscreen.html',
      reasons: ['TESTING'],
      justification: 'ignored',
    };


chrome.test.runTests([
  async function createDocumentAndEnsureItExistsAndThenClose() {
    chrome.test.assertFalse(await chrome.offscreen.hasDocument());

    await chrome.offscreen.createDocument(VALID_PARAMS);
    chrome.test.assertTrue(await chrome.offscreen.hasDocument());

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
    chrome.test.assertFalse(await chrome.offscreen.hasDocument());

    chrome.test.succeed();
  },

  async function createDocumentWithAbsoluteSameOriginUrlSucceeds() {
    chrome.test.assertFalse(await chrome.offscreen.hasDocument());

    await chrome.offscreen.createDocument(
        {
          url: chrome.runtime.getURL('offscreen.html'),
          reasons: ['TESTING'],
          justification: 'ignored',
        });
    chrome.test.assertTrue(await chrome.offscreen.hasDocument());

    // Tidy up.
    await chrome.offscreen.closeDocument();
    chrome.test.succeed();
  },

  async function createDocumentWithInvalidUrlsRejects() {
    chrome.test.assertFalse(await chrome.offscreen.hasDocument());

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
      chrome.test.assertFalse(await chrome.offscreen.hasDocument());
    }
    chrome.test.succeed();
  },

  async function cannotCreateMoreThanOneOffscreenDocument() {
    chrome.test.assertFalse(await chrome.offscreen.hasDocument());
    await chrome.offscreen.createDocument(VALID_PARAMS);
    chrome.test.assertTrue(await chrome.offscreen.hasDocument());

    await chrome.test.assertPromiseRejects(
        chrome.offscreen.createDocument(VALID_PARAMS),
        'Error: Only a single offscreen document may be created.');

    // Tidy up.
    await chrome.offscreen.closeDocument();
    chrome.test.succeed();
  },

  async function callingCloseDocumentWhenNoneOpenRejects() {
    chrome.test.assertFalse(await chrome.offscreen.hasDocument());
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

  async function callingCreateDocumentWithMultipleReasonsRejects() {
    await chrome.test.assertPromiseRejects(
        chrome.offscreen.createDocument(
        {
          url: 'offscreen.html',
          reasons: ['TESTING', 'AUDIO_PLAYBACK'],
          justification: 'ignored',
        }),
        'Error: Only a single `reason` is currently supported.');
    chrome.test.succeed();
  },

  async function nonexistentRelativePathIsAccepted() {
    chrome.test.assertFalse(await chrome.offscreen.hasDocument());
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
    chrome.test.assertTrue(await chrome.offscreen.hasDocument());

    // Tidy up.
    await chrome.offscreen.closeDocument();
    chrome.test.succeed();
  },
])

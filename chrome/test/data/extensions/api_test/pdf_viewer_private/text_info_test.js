// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  /** getTextInfo() doesn't serialize known fonts. */
  async function testSkipKnownFontIds() {
    const textarea = document.createElement('textarea');
    textarea.value = 'Test text';
    document.body.appendChild(textarea);

    const freshInfo = await chrome.pdfViewerPrivate.getTextInfo(textarea, []);
    chrome.test.assertTrue(
        freshInfo.typefaces && freshInfo.typefaces.length === 1,
        'Expected typefaces array to be populated');
    chrome.test.assertTrue(
        freshInfo.typefaces[0].serializedTypeface.byteLength > 0,
        'Expected the serializedTypeface to be non-empty');

    const cachedInfo = await chrome.pdfViewerPrivate.getTextInfo(
        textarea, [freshInfo.typefaces[0].uniqueId]);

    chrome.test.assertTrue(
        cachedInfo.typefaces && cachedInfo.typefaces.length === 0,
        'getTextInfo() failed to skip the knownFontIds');

    chrome.test.succeed();
  },
  /** getTextInfo() ignores IDs in knownFontIds not in the current result. */
  async function testIgnoreUnseenKnownFontIds() {
    const textarea = document.createElement('textarea');
    textarea.value = 'Test text';
    document.body.appendChild(textarea);

    const info = await chrome.pdfViewerPrivate.getTextInfo(textarea, [1337]);
    chrome.test.assertTrue(
        info.typefaces && info.typefaces.length === 1,
        'Expected typefaces array to be populated');
    chrome.test.assertTrue(
        info.typefaces[0].serializedTypeface.byteLength > 0,
        'Expected the serializedTypeface to be non-empty');

    chrome.test.succeed();
  },
  /** getTextInfo() throws an exception for the wrong element type. */
  async function testBadElementType() {
    const div = document.createElement('div');
    div.value = 'Test text';
    document.body.appendChild(div);

    await chrome.test.assertThrows(
        chrome.pdfViewerPrivate.getTextInfo, [div, []],
        new RegExp('Value must be an instance of HTMLTextAreaElement'));

    chrome.test.succeed();
  },
  /** getTextInfo() throws an exception for wrong arguments. */
  async function testBadArgumentCount() {
    const textarea = document.createElement('textarea');
    textarea.value = 'Test text';
    document.body.appendChild(textarea);

    await chrome.test.assertThrows(
        chrome.pdfViewerPrivate.getTextInfo, [],
        new RegExp('No matching signature'));

    await chrome.test.assertThrows(
        chrome.pdfViewerPrivate.getTextInfo, [textarea],
        new RegExp('No matching signature'));

    await chrome.test.assertThrows(
        chrome.pdfViewerPrivate.getTextInfo, [textarea, [], 123],
        new RegExp('No matching signature'));

    chrome.test.succeed();
  },
  /** getTextInfo() throws when the second arg is the wrong type. */
  async function testBadListArgumentType() {
    const textarea = document.createElement('textarea');
    textarea.value = 'Test text';
    document.body.appendChild(textarea);

    await chrome.test.assertThrows(
        chrome.pdfViewerPrivate.getTextInfo, [textarea, 1337],
        new RegExp('No matching signature'));

    await chrome.test.assertPromiseRejects(
        chrome.pdfViewerPrivate.getTextInfo(textarea, [-10]),
        new RegExp('elements must be uint32'));

    await chrome.test.assertThrows(
        chrome.pdfViewerPrivate.getTextInfo, [textarea, [10, []]],
        new RegExp('Invalid type: expected integer'));

    chrome.test.succeed();
  },
]);

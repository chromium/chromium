// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  // Tests that an error is returned when request does not include neither
  // documentId or tabId.
  async function noDocumentOrTabId() {
    const request = {};
    await chrome.test.assertPromiseRejects(
        chrome.permissions.removeSiteAccessRequest(request),
        `Error: Must specify either 'documentId' or 'tabId'.`);

    chrome.test.succeed();
  },

  // Tests that an error is returned when request includes both documentId and
  // tabId.
  async function bothDocumentOrTabId() {
    const request = {documentId: '123', tabId: 456};
    await chrome.test.assertPromiseRejects(
        chrome.permissions.removeSiteAccessRequest(request),
        `Error: Must specify either 'documentId' or 'tabId'.`);

    chrome.test.succeed();
  },

  // Tests that an error is returned when tabId in request doesn't exist.
  async function nonExistentTabId() {
    const tabId = 123;
    const request = {tabId: tabId};
    await chrome.test.assertPromiseRejects(
        chrome.permissions.removeSiteAccessRequest(request),
        `Error: No tab with ID '${tabId}'.`);

    chrome.test.succeed();
  },

  // Tests that an error is returned when documentId in request doesn't exist.
  async function nonExistentDocumentId() {
    const documentId = 'invalid id';
    const request = {documentId: documentId};
    await chrome.test.assertPromiseRejects(
        chrome.permissions.removeSiteAccessRequest(request),
        `Error: No document with ID '${documentId}'.`);

    chrome.test.succeed();
  }
])

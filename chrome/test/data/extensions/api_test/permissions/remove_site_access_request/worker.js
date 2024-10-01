// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

// Navigates to a url with `origin`.
async function navigateTo(origin) {
  const config = await chrome.test.getConfig();
  const url = `http://${origin}:${config.testServer.port}/simple.html`;
  const tab = await openTab(url);
  return tab;
}

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
  },

  // Tests that an error is returned when the request cannot be removed since
  // the request doesn't exist.
  async function nonExistentRequest() {
    const tab = await navigateTo('example.com');
    const request = {tabId: tab.id};
    await chrome.test.assertPromiseRejects(
        chrome.permissions.removeSiteAccessRequest(request),
        `Error: Extension cannot remove a site access request that doesn't ` +
            `exist.`);

    chrome.test.succeed();
  },

  // Tests that an error is returned when the request to remove has an invalid
  // pattern.
  async function invalidPattern() {
    let tab = await navigateTo('requested.com');

    const request = {tabId: tab.id, pattern: 'invalid pattern'};
    await chrome.test.assertPromiseRejects(
        chrome.permissions.removeSiteAccessRequest(request),
        `Error: Extension cannot remove a request with an invalid value for ` +
            `'pattern'.`);

    chrome.test.succeed();
  }
])

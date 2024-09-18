// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

// Navigates to an url requested by the extension and returns the opened tab.
async function navigateTo(origin) {
  const config = await chrome.test.getConfig();
  const url = `http://${origin}:${config.testServer.port}/simple.html`;
  let tab = await openTab(url);
  return tab;
}

chrome.test.runTests([
  // Tests that an error is returned when request does not include neither
  // documentId or tabId.
  async function noDocumentOrTabId() {
    const request = {};
    await chrome.test.assertPromiseRejects(
        chrome.permissions.addSiteAccessRequest(request),
        `Error: Must specify either 'documentId' or 'tabId'.`);

    chrome.test.succeed();
  },

  // Tests that an error is returned when request includes both documentId and
  // tabId.
  async function bothDocumentOrTabId() {
    const request = {documentId: '123', tabId: 456};
    await chrome.test.assertPromiseRejects(
        chrome.permissions.addSiteAccessRequest(request),
        `Error: Must specify either 'documentId' or 'tabId'.`);

    chrome.test.succeed();
  },

  // Tests that an error is returned when tabId in request doesn't exist.
  async function nonExistentTabId() {
    const tabId = 123;
    const request = {tabId: tabId};
    await chrome.test.assertPromiseRejects(
        chrome.permissions.addSiteAccessRequest(request),
        `Error: No tab with ID '${tabId}'.`);

    chrome.test.succeed();
  },

  // Tests that an error is returned when documentId in request doesn't exist.
  async function nonExistentDocumentId() {
    const documentId = 'invalid id';
    const request = {documentId: documentId};
    await chrome.test.assertPromiseRejects(
        chrome.permissions.addSiteAccessRequest(request),
        `Error: No document with ID '${documentId}'.`);

    chrome.test.succeed();
  },

  // Tests that an error is returned when the extension adds a request for a
  // tabId that it can already access its current web contents.
  async function accessAlreadyGrantedForTabId() {
    let tab = await navigateTo('requested.com');

    const request = {tabId: tab.id};
    await chrome.test.assertPromiseRejects(
        chrome.permissions.addSiteAccessRequest(request),
        `Error: Extension cannot add a site access request for a site it ` +
            `already has access to.`);

    chrome.test.succeed();
  },

  // Tests that an error is returned when the extension adds a request for a
  // documentId that it can already access its current web contents.
  async function accessAlreadyGrantedForDocumentId() {
    let tab = await navigateTo('requested.com');
    let frame = await chrome.webNavigation.getFrame({frameId: 0, tabId: tab.id})

    const request = {documentId: frame.documentId};
    await chrome.test.assertPromiseRejects(
      chrome.permissions.addSiteAccessRequest(request),
      `Error: Extension cannot add a site access request for a site it ` +
          `already has access to.`);

    chrome.test.succeed();
  },

  // Tests that an error is returned when the extension adds a request with an
  // invalid pattern.
  async function invalidPattern() {
    let tab = await navigateTo('requested.com');

    const request = {tabId: tab.id, pattern: 'invalid pattern'};
    await chrome.test.assertPromiseRejects(
        chrome.permissions.addSiteAccessRequest(request),
        `Error: Extension cannot add a request with an invalid value for ` +
            `'pattern'.`);

    chrome.test.succeed();
  }
])

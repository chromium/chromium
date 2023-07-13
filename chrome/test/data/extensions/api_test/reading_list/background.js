// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([

  async function testAddEntryFunction() {
    const entry = {
      url: 'https://www.example.com',
      title: 'example of title',
      hasBeenRead: false
    };
    await chrome.readingList.addEntry(entry);
    await chrome.test.assertPromiseRejects(
        chrome.readingList.addEntry(entry), 'Error: Duplicate URL.');
    chrome.test.succeed();
  },

  async function testAddEntryInvalidURLError() {
    const entry = {
      url: 'Invalid URL',
      title: 'example of title',
      hasBeenRead: false
    };
    await chrome.test.assertPromiseRejects(
        chrome.readingList.addEntry(entry), 'Error: URL is not valid.');
    chrome.test.succeed();
  },

  async function TestAddEntryNotSupportedURLError() {
    const entry = {
      url: 'chrome://example',
      title: 'example of title',
      hasBeenRead: false
    };
    await chrome.test.assertPromiseRejects(
        chrome.readingList.addEntry(entry), 'Error: URL is not supported.');
    chrome.test.succeed();
  },

  async function testRemoveEntryFunction() {
    const entry = {
      url: 'https://www.example.com'
    };
    await chrome.readingList.removeEntry(entry);
    await chrome.test.assertPromiseRejects(
        chrome.readingList.removeEntry(entry), 'Error: URL not found.');
    chrome.test.succeed();
  },

  async function testRemoveEntryInvalidURLError() {
    const entry = {
      url: 'Invalid URL'
    };
    await chrome.test.assertPromiseRejects(
        chrome.readingList.removeEntry(entry), 'Error: URL is not valid.');
    chrome.test.succeed();
  },

  async function TestRemoveEntryNotSupportedURLError() {
    const entry = {
      url: 'chrome://example'
    };
    await chrome.test.assertPromiseRejects(
        chrome.readingList.removeEntry(entry), 'Error: URL is not supported.');
    chrome.test.succeed();
  },

]);

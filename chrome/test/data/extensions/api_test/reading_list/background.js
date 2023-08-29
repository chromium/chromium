// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let readingList = chrome.readingList;
chrome.test.runTests([

  async function testAddEntryFunction() {
    const entry = {
      url: 'https://www.example.com',
      title: 'example of title',
      hasBeenRead: false
    };
    await readingList.addEntry(entry);
    await chrome.test.assertPromiseRejects(
        readingList.addEntry(entry), 'Error: Duplicate URL.');
    chrome.test.succeed();
  },

  async function testAddEntryInvalidURLError() {
    const entry = {
      url: 'Invalid URL',
      title: 'example of title',
      hasBeenRead: false
    };
    await chrome.test.assertPromiseRejects(
        readingList.addEntry(entry), 'Error: URL is not valid.');
    chrome.test.succeed();
  },

  async function testAddEntryNotSupportedURLError() {
    const entry = {
      url: 'chrome://example',
      title: 'example of title',
      hasBeenRead: false
    };
    await chrome.test.assertPromiseRejects(
        readingList.addEntry(entry), 'Error: URL is not supported.');
    chrome.test.succeed();
  },

  async function testRemoveEntryFunction() {
    const entry = {
      url: 'https://www.example.com'
    };
    await readingList.removeEntry(entry);
    await chrome.test.assertPromiseRejects(
        readingList.removeEntry(entry), 'Error: URL not found.');
    chrome.test.succeed();
  },

  async function testRemoveEntryInvalidURLError() {
    const entry = {
      url: 'Invalid URL'
    };
    await chrome.test.assertPromiseRejects(
        readingList.removeEntry(entry), 'Error: URL is not valid.');
    chrome.test.succeed();
  },

  async function testRemoveEntryNotSupportedURLError() {
    const entry = {
      url: 'chrome://example'
    };
    await chrome.test.assertPromiseRejects(
        readingList.removeEntry(entry), 'Error: URL is not supported.');
    chrome.test.succeed();
  },

  async function testUpdateEntryFunction() {
    let entry = {
      url: 'https://www.example.com',
      title: 'Title',
      hasBeenRead: true
    };
    await readingList.addEntry(entry);
    entry.title = 'New title';
    await readingList.updateEntry(entry);
    chrome.test.succeed();
  },

  async function testNoFeaturesProvidedUpdateEntry() {
    const entry = {
      url: 'https://www.example.com',
    };
    await chrome.test.assertPromiseRejects(
        readingList.updateEntry(entry),
        'Error: At least one of `title` or `hasBeenRead` must be provided.');
    chrome.test.succeed();
  },

  async function testUpdateEntryInvalidURLError() {
    const entry = {url: 'Invalid URL', title: 'example of title'};
    await chrome.test.assertPromiseRejects(
        readingList.updateEntry(entry), 'Error: URL is not valid.');
    chrome.test.succeed();
  },

  async function testUpdateEntryNotSupportedURLError() {
    const entry = {url: 'chrome://example', title: 'example of title'};
    await chrome.test.assertPromiseRejects(
        readingList.updateEntry(entry), 'Error: URL is not supported.');
    chrome.test.succeed();
  },

  async function testQueryFunction() {
    let entry = {
      url: 'https://www.example2.com',
      title: 'Example',
      hasBeenRead: false
    };
    readingList.addEntry(entry);
    entry.url = 'https://www.example3.com';
    readingList.addEntry(entry);

    let query = {title: 'Example'};
    let entries = await readingList.query(query);
    chrome.test.assertEq(entries.length, 2);

    // The times are arbitrary and thus hard to test especially since the
    // function call is asynchronous. For this reason, we reuse the times from
    // the entries themselves.
    const expectedResult = [
      {
        'url': 'https://www.example2.com/',
        'title': 'Example',
        'hasBeenRead': false,
        'creationTime': entries[0].creationTime,
        'lastUpdateTime': entries[0].lastUpdateTime,
      },
      {
        'url': 'https://www.example3.com/',
        'title': 'Example',
        'hasBeenRead': false,
        'creationTime': entries[1].creationTime,
        'lastUpdateTime': entries[1].lastUpdateTime,
      }
    ];
    chrome.test.assertEq(entries, expectedResult);

    query.title = 'Null';
    entries = await readingList.query(query);
    chrome.test.assertEq(entries.length, 0);
    chrome.test.succeed();
  },

  async function testQueryInvalidURLError() {
    const entry = {url: 'Invalid URL', title: 'example of title'};
    await chrome.test.assertPromiseRejects(
        readingList.query(entry), 'Error: URL is not valid.');
    chrome.test.succeed();
  },

  async function testQueryNotSupportedURLError() {
    const entry = {url: 'chrome://example', title: 'example of title'};
    await chrome.test.assertPromiseRejects(
        readingList.query(entry), 'Error: URL is not supported.');
    chrome.test.succeed();
  },

]);

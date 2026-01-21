// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarksEditDialogElement} from 'chrome://bookmarks/bookmarks.js';
import {BookmarksApiProxyImpl, MAX_BOOKMARK_INPUT_LENGTH, normalizeNode, setDebouncerForTesting} from 'chrome://bookmarks/bookmarks.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';
import {createFolder, createItem, replaceBody} from './test_util.js';

suite('<bookmarks-edit-dialog>', function() {
  let bookmarksApi: TestBookmarksApiProxy;
  let dialog: BookmarksEditDialogElement;
  let lastUpdate: {id: string, edit: {url?: string, title?: string}};

  suiteSetup(function() {
    chrome.bookmarks.update = function(id, edit) {
      lastUpdate.id = id;
      lastUpdate.edit = edit;
      return Promise.resolve({id: '', title: ''});
    };
  });

  setup(function() {
    bookmarksApi = new TestBookmarksApiProxy();
    BookmarksApiProxyImpl.setInstance(bookmarksApi);
    lastUpdate = {id: '', edit: {}};
    dialog = document.createElement('bookmarks-edit-dialog');
    replaceBody(dialog);
  });

  test('editing an item shows the url field', async () => {
    const item = normalizeNode(createItem('0'));
    dialog.showEditDialog(item);
    await microtasksFinished();

    assertFalse(dialog.$.url.hidden);
  });

  test('editing a folder hides the url field', async () => {
    const folder = normalizeNode(createFolder('0', []));
    dialog.showEditDialog(folder);
    await microtasksFinished();

    assertTrue(dialog.$.url.hidden);
  });

  test('adding a folder hides the url field', async () => {
    dialog.showAddDialog(true, '1');
    await microtasksFinished();
    assertTrue(dialog.$.url.hidden);
  });

  test('editing passes the correct details to the update', async function() {
    // Editing an item without changing anything.
    const item = normalizeNode(
        createItem('1', {url: 'http://website.com', title: 'website'}));
    dialog.showEditDialog(item);
    await microtasksFinished();

    dialog.$.saveButton.click();
    assertEquals(item.id, lastUpdate.id);
    assertEquals(item.url, lastUpdate.edit.url);
    assertEquals(item.title, lastUpdate.edit.title);

    // Editing a folder, changing the title.
    const folder = normalizeNode(createFolder('2', [], {title: 'Cool Sites'}));
    dialog.showEditDialog(folder);
    await microtasksFinished();
    dialog.$.name.value = 'Awesome websites';
    await microtasksFinished();

    dialog.$.saveButton.click();
    assertEquals(folder.id, lastUpdate.id);
    assertEquals(undefined, lastUpdate.edit.url);
    assertEquals('Awesome websites', lastUpdate.edit.title);
  });

  test('add passes the correct details to the backend', async function() {
    dialog.showAddDialog(false, '1');
    await microtasksFinished();

    dialog.$.name.value = 'Permission Site';
    await microtasksFinished();
    dialog.$.url.value = 'permission.site';
    await microtasksFinished();

    setDebouncerForTesting();

    dialog.$.saveButton.click();

    const args = await bookmarksApi.whenCalled('create');

    assertEquals('1', args.parentId);
    assertEquals('http://permission.site', args.url);
    assertEquals('Permission Site', args.title);
  });

  function setUrlValue(value: string) {
    dialog.$.url.value = value;
    dialog.$.url.dispatchEvent(new CustomEvent('value-changed', {
      bubbles: true,
      composed: true,
      detail: {value: value},
    }));
  }

  test('validates urls correctly', async () => {
    setUrlValue('http://www.example.com');
    assertTrue(dialog.validateUrl());

    setUrlValue('https://a@example.com:8080');
    assertTrue(dialog.validateUrl());

    setUrlValue('example.com');
    assertTrue(dialog.validateUrl());
    await microtasksFinished();
    assertEquals('http://example.com', dialog.$.url.value);

    setUrlValue('');
    assertFalse(dialog.validateUrl());

    setUrlValue('~~~example.com~~~');
    assertTrue(dialog.validateUrl());

    setUrlValue('^^^example.com^^^');
    assertFalse(dialog.validateUrl());
    setUrlValue('a'.repeat(MAX_BOOKMARK_INPUT_LENGTH + 1));
    assertFalse(dialog.validateUrl());

    // Case: Exactly 500KB, valid URL.
    // "http://" is 7 chars. "a" * (500*1024 - 7)
    const validUrl = 'http://' +
        'a'.repeat(MAX_BOOKMARK_INPUT_LENGTH - 7);
    setUrlValue(validUrl);
    assertTrue(dialog.validateUrl());
  });

  async function testPasteTruncation(
      input: HTMLInputElement, maxLength: number) {
    const longText = 'a'.repeat(maxLength + 100);

    // Mock the clipboard event.
    const clipboardData = new DataTransfer();
    clipboardData.setData('text/plain', longText);
    const pasteEvent = new ClipboardEvent('paste', {
      bubbles: true,
      cancelable: true,
      composed: true,
      clipboardData: clipboardData,
    });

    input.dispatchEvent(pasteEvent);
    await microtasksFinished();

    // Verify the text was truncated.
    assertEquals(maxLength, input.value.length);
    assertEquals(longText.substring(0, maxLength), input.value);
  }

  test('truncates long pasted URLs', async () => {
    dialog.showAddDialog(false, '1');
    await microtasksFinished();

    await testPasteTruncation(
        dialog.$.url.inputElement, MAX_BOOKMARK_INPUT_LENGTH);
  });

  test('truncates long pasted titles', async () => {
    dialog.showAddDialog(false, '1');
    await microtasksFinished();

    await testPasteTruncation(
        dialog.$.name.inputElement, MAX_BOOKMARK_INPUT_LENGTH);
  });

  test('should truncate title on save if it exceeds the limit', async () => {
    dialog.showAddDialog(false, '1');
    await microtasksFinished();

    const longTitle = 'a'.repeat(MAX_BOOKMARK_INPUT_LENGTH + 100);
    // Directly set the private property to avoid performance issues with
    // rendering extremely long strings in cr-input during tests.
    dialog['titleValue_'] = longTitle;
    await microtasksFinished();

    dialog.$.url.value = 'http://example.com';
    dialog.$.url.dispatchEvent(new CustomEvent('value-changed', {
      bubbles: true,
      composed: true,
      detail: {value: 'http://example.com'},
    }));
    await microtasksFinished();

    setDebouncerForTesting();

    dialog.$.saveButton.click();

    const args = await bookmarksApi.whenCalled('create');
    assertEquals(MAX_BOOKMARK_INPUT_LENGTH, args.title.length);
  });

  test('doesn\'t save when URL is invalid', async () => {
    const item = normalizeNode(createItem('0'));
    dialog.showEditDialog(item);
    await microtasksFinished();

    dialog.$.url.value = '';
    await microtasksFinished();
    dialog.$.saveButton.click();

    assertTrue(dialog.$.url.invalid);
    assertTrue(dialog.$.dialog.open);
  });
});

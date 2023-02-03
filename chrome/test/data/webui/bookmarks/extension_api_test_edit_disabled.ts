// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Bookmark Manager API test for Chrome.
import {assertEquals, assertFalse, assertNotReached} from 'chrome://webui-test/chai_assert.js';

suite('Bookmarks API with edit disabled', () => {
  const bookmarkManager = chrome.bookmarkManagerPrivate;

  const ERROR = 'Bookmark editing is disabled.';
  let bar: chrome.bookmarks.BookmarkTreeNode;
  let folder: chrome.bookmarks.BookmarkTreeNode;
  let aaa: chrome.bookmarks.BookmarkTreeNode;
  let bbb: chrome.bookmarks.BookmarkTreeNode;

  // Bookmark model within this test:
  //  <root>/
  //    Bookmarks Bar/
  //      Folder/
  //        "BBB"
  //      "AAA"
  //    Other Bookmarks/

  test('verify bookmark model', async function() {
    const result = await chrome.bookmarks.getTree();
    assertEquals(1, result.length);
    const root = result[0] as chrome.bookmarks.BookmarkTreeNode;
    assertEquals(2, root.children!.length);
    bar = root.children![0] as chrome.bookmarks.BookmarkTreeNode;
    assertEquals(2, bar.children!.length);
    folder = bar.children![0] as chrome.bookmarks.BookmarkTreeNode;
    aaa = bar.children![1] as chrome.bookmarks.BookmarkTreeNode;
    assertEquals('Folder', folder.title);
    assertEquals('AAA', aaa.title);
    bbb = folder.children![0] as chrome.bookmarks.BookmarkTreeNode;
    assertEquals('BBB', bbb.title);
  });

  test('disables create', function() {
    return chrome.bookmarks.create({parentId: bar.id, title: 'Folder2'})
        .then(_ => assertNotReached())
        .catch(error => assertEquals(error.message, ERROR));
  });

  test('disables move', function() {
    return chrome.bookmarks
        .move(aaa.id, {parentId: folder.id, index: undefined})
        .then(_ => assertNotReached())
        .catch(error => assertEquals(error.message, ERROR));
  });

  test('disables remove', function() {
    return chrome.bookmarks.remove(aaa.id)
        .then(() => assertNotReached())
        .catch(error => assertEquals(error.message, ERROR));
  });

  test('disables remove tree', function() {
    return chrome.bookmarks.removeTree(folder.id)
        .then(() => assertNotReached())
        .catch(error => assertEquals(error.message, ERROR));
  });

  test('disables update', function() {
    return chrome.bookmarks.update(aaa.id, {title: 'CCC'})
        .then(_ => assertNotReached())
        .catch(error => assertEquals(error.message, ERROR));
  });

  test('disables import', function() {
    return bookmarkManager.import()
        .then(_ => assertNotReached())
        .catch(error => assertEquals(error.message, ERROR));
  });

  test('disables cut', function() {
    return bookmarkManager.cut([bbb.id])
        .then(_ => assertNotReached())
        .catch(error => assertEquals(error.message, ERROR));
  });

  test('disables can paste', function() {
    return bookmarkManager.canPaste(folder.id).then(result => {
      assertFalse(result, 'Should not be able to paste bookmarks');
    });
  });

  test('disables paste', function() {
    return bookmarkManager.paste(folder.id, [bbb.id])
        .then(_ => assertNotReached())
        .catch(error => assertEquals(error.message, ERROR));
  });
});

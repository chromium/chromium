// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Bookmark Manager API test for Chrome.
import {simulateChromeExtensionAPITest} from 'chrome://test/bookmarks/test_util.js';

test('bookmarkManagerPrivate with edit disabled', async () => {
  const bookmarkManager = chrome.bookmarkManagerPrivate;
  const {pass, fail, runTests} = simulateChromeExtensionAPITest();

  const ERROR = 'Bookmark editing is disabled.';
  let bar;
  let folder;
  let aaa;
  let bbb;

  // Bookmark model within this test:
  //  <root>/
  //    Bookmarks Bar/
  //      Folder/
  //        "BBB"
  //      "AAA"
  //    Other Bookmarks/

  const tests = [
    function verifyModel() {
      chrome.bookmarks.getTree(pass(function(result) {
        assertEquals(1, result.length);
        const root = result[0];
        assertEquals(2, root.children.length);
        bar = root.children[0];
        assertEquals(2, bar.children.length);
        folder = bar.children[0];
        aaa = bar.children[1];
        assertEquals('Folder', folder.title);
        assertEquals('AAA', aaa.title);
        bbb = folder.children[0];
        assertEquals('BBB', bbb.title);
      }));
    },

    function createDisabled() {
      chrome.bookmarks.create(
          {parentId: bar.id, title: 'Folder2'}, fail(ERROR));
    },

    function moveDisabled() {
      chrome.bookmarks.move(aaa.id, {parentId: folder.id}, fail(ERROR));
    },

    function removeDisabled() {
      chrome.bookmarks.remove(aaa.id, fail(ERROR));
    },

    function removeTreeDisabled() {
      chrome.bookmarks.removeTree(folder.id, fail(ERROR));
    },

    function updateDisabled() {
      chrome.bookmarks.update(aaa.id, {title: 'CCC'}, fail(ERROR));
    },

    function importDisabled() {
      chrome.bookmarks.import(fail(ERROR));
    },

    function cutDisabled() {
      bookmarkManager.cut([bbb.id], fail(ERROR));
    },

    function canPasteDisabled() {
      bookmarkManager.canPaste(
          folder.id, pass(function(result) {
            assertFalse(result, 'Should not be able to paste bookmarks');
          }));
    },

    function pasteDisabled() {
      bookmarkManager.paste(folder.id, [bbb.id], fail(ERROR));
    },

    function editDisabled() {
      bookmarkManager.canEdit(pass(function(result) {
        assertFalse(result, 'Should not be able to edit bookmarks');
      }));
    },
  ];

  runTests(tests);
});

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Bookmark Manager API test for Chrome.
import {simulateChromeExtensionAPITest} from './test_util.js';

test('bookmarkManagerPrivate', async () => {
  const bookmarkManager = chrome.bookmarkManagerPrivate;
  const {pass, fail, runTests} = simulateChromeExtensionAPITest();

  let fooNode;
  let fooNode2;
  let barNode;
  let gooNode;
  let count;
  let emptyFolder;
  let emptyFolder2;
  let folder;
  let nodeA;
  let nodeB;
  let childFolder;
  let grandChildFolder;
  let childNodeA;
  let childNodeB;

  function doCopy() {
    bookmarkManager.copy.apply(null, arguments);
  }
  function doCut() {
    bookmarkManager.cut.apply(null, arguments);
  }
  function doPaste() {
    bookmarkManager.paste.apply(null, arguments);
  }

  const tests = [
    function sortChildren() {
      folder = {parentId: '1', title: 'Folder'};
      nodeA = {title: 'a', url: 'http://www.example.com/a'};
      nodeB = {title: 'b', url: 'http://www.example.com/b'};
      chrome.bookmarks.create(
          folder, pass(function(result) {
            assertTrue(!!result.id);
            folder.id = result.id;
            nodeA.parentId = folder.id;
            nodeB.parentId = folder.id;

            chrome.bookmarks.create(nodeB, pass(function(result) {
                                      nodeB.id = result.id;
                                    }));
            chrome.bookmarks.create(nodeA, pass(function(result) {
                                      nodeA.id = result.id;
                                    }));
          }));
    },

    function sortChildren2() {
      bookmarkManager.sortChildren(folder.id);

      chrome.bookmarks.getChildren(folder.id, pass(function(children) {
                                     assertEquals(nodeA.id, children[0].id);
                                     assertEquals(nodeB.id, children[1].id);
                                   }));
    },

    function setupSubtree() {
      childFolder = {parentId: folder.id, title: 'Child Folder'};
      childNodeA = {
        title: 'childNodeA',
        url: 'http://www.example.com/childNodeA',
      };
      childNodeB = {
        title: 'childNodeB',
        url: 'http://www.example.com/childNodeB',
      };
      grandChildFolder = {title: 'grandChildFolder'};
      chrome.bookmarks.create(
          childFolder, pass(function(result) {
            childFolder.id = result.id;
            childNodeA.parentId = childFolder.id;
            childNodeB.parentId = childFolder.id;
            grandChildFolder.parentId = childFolder.id;

            chrome.bookmarks.create(childNodeA, pass(function(result) {
                                      childNodeA.id = result.id;
                                    }));
            chrome.bookmarks.create(childNodeB, pass(function(result) {
                                      childNodeB.id = result.id;
                                    }));
            chrome.bookmarks.create(grandChildFolder, pass(function(result) {
                                      grandChildFolder.id = result.id;
                                    }));
          }));
    },

    function getSubtree() {
      bookmarkManager.getSubtree(childFolder.id, false, pass(function(result) {
                                   const children = result[0].children;
                                   assertEquals(3, children.length);
                                   assertEquals(childNodeA.id, children[0].id);
                                   assertEquals(childNodeB.id, children[1].id);
                                   assertEquals(
                                       grandChildFolder.id, children[2].id);
                                 }));
    },

    function getSubtreeFoldersOnly() {
      bookmarkManager.getSubtree(childFolder.id, true, pass(function(result) {
                                   const children = result[0].children;
                                   assertEquals(1, children.length);
                                   assertEquals(
                                       grandChildFolder.id, children[0].id);
                                 }));
    },

    // The clipboard test is split into different parts to allow asynchronous
    // operations to finish.
    function clipboard() {
      // Create a new bookmark.
      fooNode = {
        parentId: '1',
        title: 'Foo',
        url: 'http://www.example.com/foo',
      };

      emptyFolder = {parentId: '1', title: 'Empty Folder'};

      chrome.bookmarks.create(fooNode, pass(function(result) {
                                fooNode.id = result.id;
                                fooNode.index = result.index;
                                count = result.index + 1;
                              }));

      chrome.bookmarks.create(emptyFolder, pass(function(result) {
                                emptyFolder.id = result.id;
                                emptyFolder.index = result.index;
                                count = result.index + 1;
                              }));

      // Create a couple more bookmarks to test proper insertion of pasted
      // items.
      barNode = {
        parentId: '1',
        title: 'Bar',
        url: 'http://www.example.com/bar',
      };

      chrome.bookmarks.create(barNode, pass(function(result) {
                                barNode.id = result.id;
                                barNode.index = result.index;
                                count = result.index + 1;
                              }));

      gooNode = {
        parentId: '1',
        title: 'Goo',
        url: 'http://www.example.com/goo',
      };

      chrome.bookmarks.create(gooNode, pass(function(result) {
                                gooNode.id = result.id;
                                gooNode.index = result.index;
                                count = result.index + 1;
                              }));
    },

    function clipboard2() {
      // Copy the fooNode.
      doCopy([fooNode.id]);

      // Ensure canPaste is now true.
      bookmarkManager.canPaste('1', pass(function(result) {
                                 assertTrue(
                                     result, 'Should be able to paste now');
                               }));

      // Paste it.
      doPaste('1');

      // Ensure it got added at the end.
      chrome.bookmarks.getChildren(
          '1', pass(function(result) {
            count++;
            assertEquals(count, result.length);

            fooNode2 = result[result.length - 1];

            assertEquals(fooNode.title + ' (1)', fooNode2.title);
            assertEquals(fooNode.url, fooNode2.url);
            assertEquals(fooNode.parentId, fooNode2.parentId);
          }));
    },

    function clipboard3() {
      // Cut fooNode chrome.bookmarks.
      doCut([fooNode.id, fooNode2.id]);

      // Ensure count decreased by 2.
      chrome.bookmarks.getChildren('1', pass(function(result) {
                                     count -= 2;
                                     assertEquals(count, result.length);
                                   }));

      // Ensure canPaste is still true.
      bookmarkManager.canPaste('1', pass(function(result) {
                                 assertTrue(
                                     result, 'Should be able to paste now');
                               }));
    },

    function clipboard4() {
      // Paste the cut bookmarks at a specific position between bar and goo.
      doPaste('1', [barNode.id]);

      // Check that the two bookmarks were pasted after bar.
      chrome.bookmarks.getChildren(
          '1', pass(function(result) {
            count += 2;
            assertEquals(count, result.length);

            // Look for barNode's index.
            let barIndex;
            for (barIndex = 0; barIndex < result.length; barIndex++) {
              if (result[barIndex].id === barNode.id) {
                break;
              }
            }
            assertTrue(barIndex + 2 < result.length);

            const last = result[barIndex + 1];
            const last2 = result[barIndex + 2];
            assertEquals(fooNode.title, last.title);
            assertEquals(fooNode.url, last.url);
            assertEquals(fooNode.parentId, last.parentId);
            assertEquals(last.title + ' (1)', last2.title);
            assertEquals(last.url, last2.url);
            assertEquals(last.parentId, last2.parentId);

            // Remember last2 id, so we can use it in next test.
            fooNode2.id = last2.id;
          }));
    },

    // Ensure we can copy empty folders
    function clipboard5() {
      // Copy it.
      doCopy([emptyFolder.id]);

      // Ensure canPaste is now true.
      bookmarkManager.canPaste('1', pass(function(result) {
                                 assertTrue(
                                     result, 'Should be able to paste now');
                               }));

      // Paste it at the end of a multiple selection.
      doPaste('1', [barNode.id, fooNode2.id]);

      // Ensure it got added at the right place.
      chrome.bookmarks.getChildren(
          '1', pass(function(result) {
            count++;
            assertEquals(count, result.length);

            // Look for fooNode2's index.
            let foo2Index;
            for (foo2Index = 0; foo2Index < result.length; foo2Index++) {
              if (result[foo2Index].id === fooNode2.id) {
                break;
              }
            }
            assertTrue(foo2Index + 1 < result.length);

            emptyFolder2 = result[foo2Index + 1];

            assertEquals(emptyFolder2.title, emptyFolder.title);
            assertEquals(emptyFolder2.url, emptyFolder.url);
            assertEquals(emptyFolder2.parentId, emptyFolder.parentId);
          }));
    },

    function clipboard6() {
      // Verify that we can't cut managed folders.
      chrome.bookmarks.getChildren(
          '4', pass(function(result) {
            assertEquals(2, result.length);
            const error = 'Can\'t modify managed bookmarks.';
            bookmarkManager.cut([result[0].id], fail(error));

            // Copying is fine.
            bookmarkManager.copy([result[0].id], pass(() => {}));

            // Pasting to a managed folder is not allowed.
            assertTrue(result[1].url === undefined);

            bookmarkManager.canPaste(
                result[1].id, pass(function(result) {
                  assertFalse(
                      result,
                      'Should not be able to paste to managed folders.');
                }));

            bookmarkManager.paste(result[1].id, fail(error));
          }));
    },
  ];

  runTests(tests);
});

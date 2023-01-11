// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Bookmark Manager API test for Chrome.
import {assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';

const bookmarkManager = chrome.bookmarkManagerPrivate;

function doCopy(ids: string[]): Promise<void> {
  return bookmarkManager.copy(ids);
}

function doCut(ids: string[]): Promise<void> {
  return bookmarkManager.cut(ids);
}

function doPaste(parentId: string, selectedIdList?: string[]): Promise<void> {
  return bookmarkManager.paste(parentId, selectedIdList);
}

suite('bookmarks extension API create and sort test', function() {
  let folder: chrome.bookmarks.BookmarkTreeNode;
  let nodeA: chrome.bookmarks.BookmarkTreeNode;
  let nodeB: chrome.bookmarks.BookmarkTreeNode;
  let childFolder: chrome.bookmarks.BookmarkTreeNode;
  let grandChildFolder: chrome.bookmarks.BookmarkTreeNode;
  let childNodeA: chrome.bookmarks.BookmarkTreeNode;
  let childNodeB: chrome.bookmarks.BookmarkTreeNode;

  test('sortChildren', async function() {
    const folderDetails:
        chrome.bookmarks.CreateDetails = {parentId: '1', title: 'Folder'};
    const nodeADetails: chrome.bookmarks.CreateDetails = {
      title: 'a',
      url: 'http://www.example.com/a',
    };
    const nodeBDetails: chrome.bookmarks.CreateDetails = {
      title: 'b',
      url: 'http://www.example.com/b',
    };
    folder = await chrome.bookmarks.create(folderDetails);
    assertTrue(!!folder.id);
    nodeADetails.parentId = folder.id;
    nodeBDetails.parentId = folder.id;

    nodeB = await chrome.bookmarks.create(nodeBDetails);
    nodeA = await chrome.bookmarks.create(nodeADetails);
  });

  test('sortChildren2', async function() {
    bookmarkManager.sortChildren(folder.id);

    const children = await chrome.bookmarks.getChildren(folder.id);
    assertEquals(nodeA.id, children[0]!.id);
    assertEquals(nodeB.id, children[1]!.id);
  });

  test('setupSubtree', async function() {
    const childFolderDetails: chrome.bookmarks.CreateDetails = {
      parentId: folder.id,
      title: 'Child Folder',
    };
    const childNodeADetails: chrome.bookmarks.CreateDetails = {
      title: 'childNodeA',
      url: 'http://www.example.com/childNodeA',
    };
    const childNodeBDetails: chrome.bookmarks.CreateDetails = {
      title: 'childNodeB',
      url: 'http://www.example.com/childNodeB',
    };
    const grandChildFolderDetails:
        chrome.bookmarks.CreateDetails = {title: 'grandChildFolder'};
    childFolder = await chrome.bookmarks.create(childFolderDetails);
    childNodeADetails.parentId = childFolder.id;
    childNodeBDetails.parentId = childFolder.id;
    grandChildFolderDetails.parentId = childFolder.id;

    childNodeA = await chrome.bookmarks.create(childNodeADetails);

    childNodeB = await chrome.bookmarks.create(childNodeBDetails);

    grandChildFolder = await chrome.bookmarks.create(grandChildFolderDetails);
  });

  test('getSubtree', async function() {
    const result = await bookmarkManager.getSubtree(childFolder.id, false);
    const children = result[0]!.children!;
    assertEquals(3, children.length);
    assertEquals(childNodeA.id, children[0]!.id);
    assertEquals(childNodeB.id, children[1]!.id);
    assertEquals(grandChildFolder.id, children[2]!.id);
  });

  test('getSubtreeFoldersOnly', async function() {
    const result = await bookmarkManager.getSubtree(childFolder.id, true);
    const children = result[0]!.children!;
    assertEquals(1, children.length);
    assertEquals(grandChildFolder.id, children[0]!.id);
  });
});

suite('bookmarks extension API clipboard test', function() {
  let fooNode: chrome.bookmarks.BookmarkTreeNode;
  let fooNode2: chrome.bookmarks.BookmarkTreeNode;
  let barNode: chrome.bookmarks.BookmarkTreeNode;
  let gooNode: chrome.bookmarks.BookmarkTreeNode;
  let count: number;
  let emptyFolder: chrome.bookmarks.BookmarkTreeNode;
  let emptyFolder2: chrome.bookmarks.BookmarkTreeNode;

  suiteSetup(async function() {
    // Create a new bookmark.
    const fooNodeDetails: chrome.bookmarks.CreateDetails = {
      parentId: '1',
      title: 'Foo',
      url: 'http://www.example.com/foo',
    };

    const emptyFolderDetails:
        chrome.bookmarks.CreateDetails = {parentId: '1', title: 'Empty Folder'};

    const barNodeDetails: chrome.bookmarks.CreateDetails = {
      parentId: '1',
      title: 'Bar',
      url: 'http://www.example.com/bar',
    };

    const gooNodeDetails: chrome.bookmarks.CreateDetails = {
      parentId: '1',
      title: 'Goo',
      url: 'http://www.example.com/goo',
    };

    fooNode = await chrome.bookmarks.create(fooNodeDetails);
    count = fooNode.index! + 1;

    emptyFolder = await chrome.bookmarks.create(emptyFolderDetails);
    count = emptyFolder.index! + 1;

    // Create a couple more bookmarks to test proper insertion of
    // pasted items.
    barNode = await chrome.bookmarks.create(barNodeDetails);
    count = barNode.index! + 1;

    gooNode = await chrome.bookmarks.create(gooNodeDetails);
    count = gooNode.index! + 1;
  });

  test('copies and pastes', async function() {
    // Copy the fooNode.
    await doCopy([fooNode.id]);

    // Ensure canPaste is now true.
    const canPaste = await bookmarkManager.canPaste('1');
    assertTrue(canPaste, 'Should be able to paste now');

    // Paste it.
    await doPaste('1');

    // Ensure it got added at the end.
    const result = await chrome.bookmarks.getChildren('1');
    count++;
    assertEquals(count, result.length);

    fooNode2 = result[result.length - 1]!;

    assertEquals(fooNode.title + ' (1)', fooNode2.title);
    assertEquals(fooNode.url, fooNode2.url);
    assertEquals(fooNode.parentId, fooNode2.parentId);
  });

  test('cuts bookmarks', async function() {
    // Cut fooNode chrome.bookmarks.
    await doCut([fooNode.id, fooNode2.id]);

    // Ensure count decreased by 2.
    const result = await chrome.bookmarks.getChildren('1');
    count -= 2;
    assertEquals(count, result.length);
    // Ensure canPaste is still true.
    const canPaste = await bookmarkManager.canPaste('1');
    bookmarkManager.canPaste('1');
    assertTrue(canPaste, 'Should be able to paste now');
  });

  test('pastes cut bookmarks', async function() {
    // Paste the cut bookmarks at a specific position between bar and goo.
    await doPaste('1', [barNode.id]);

    // Check that the two bookmarks were pasted after bar.
    const result = await chrome.bookmarks.getChildren('1');
    count += 2;
    assertEquals(count, result.length);

    // Look for barNode's index.
    let barIndex;
    for (barIndex = 0; barIndex < result.length; barIndex++) {
      if (result[barIndex]!.id === barNode.id) {
        break;
      }
    }
    assertTrue(barIndex + 2 < result.length);

    const last = result[barIndex + 1]!;
    const last2 = result[barIndex + 2]!;
    assertEquals(fooNode.title, last.title);
    assertEquals(fooNode.url, last.url);
    assertEquals(fooNode.parentId, last.parentId);
    assertEquals(last.title + ' (1)', last2.title);
    assertEquals(last.url, last2.url);
    assertEquals(last.parentId, last2.parentId);

    // Remember last2 id, so we can use it in next test.
    fooNode2.id = last2.id;
  });

  // Ensure we can copy empty folders
  test('enables paste', async function() {
    // Copy it.
    await doCopy([emptyFolder.id]);

    // Ensure canPaste is now true.
    const canPaste = await bookmarkManager.canPaste('1');
    assertTrue(canPaste, 'Should be able to paste now');

    // Paste it at the end of a multiple selection.
    await doPaste('1', [barNode.id, fooNode2.id]);

    // Ensure it got added at the right place.
    const result = await chrome.bookmarks.getChildren('1');
    count++;
    assertEquals(count, result.length);

    // Look for fooNode2's index.
    let foo2Index;
    for (foo2Index = 0; foo2Index < result.length; foo2Index++) {
      if (result[foo2Index]!.id === fooNode2.id) {
        break;
      }
    }
    assertTrue(foo2Index + 1 < result.length);

    emptyFolder2 = result[foo2Index + 1]!;

    assertEquals(emptyFolder2.title, emptyFolder.title);
    assertEquals(emptyFolder2.url, emptyFolder.url);
    assertEquals(emptyFolder2.parentId, emptyFolder.parentId);
  });

  test('can\'t modify managed bookmarks', async function() {
    // Verify that we can't cut managed folders.
    const result = await chrome.bookmarks.getChildren('4');
    assertEquals(2, result.length);
    const error = 'Can\'t modify managed bookmarks.';
    try {
      await bookmarkManager.cut([result[0]!.id]);
      assertNotReached();
    } catch (e: any) {
      assertEquals(e.message, error);
    }

    // Copying is fine.
    await bookmarkManager.copy([result[0]!.id]);

    // Pasting to a managed folder is not allowed.
    assertTrue(result[1]!.url === undefined);

    const canPaste = await bookmarkManager.canPaste(result[1]!.id);
    assertFalse(canPaste, 'Should not be able to paste to managed folders.');

    try {
      await bookmarkManager.paste(result[1]!.id, undefined);
      assertNotReached();
    } catch (e: any) {
      assertEquals(e.message, error);
    }
  });
});

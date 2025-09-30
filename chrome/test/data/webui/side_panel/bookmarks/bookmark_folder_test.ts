// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bookmarks-side-panel.top-chrome/bookmark_folder.js';

import {BookmarkFolderElement, FOLDER_OPEN_CHANGED_EVENT, getBookmarkFromElement} from 'chrome://bookmarks-side-panel.top-chrome/bookmark_folder.js';
import {ActionSource} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
import {BookmarksApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';

suite('SidePanelBookmarkFolderTest', () => {
  let bookmarkFolder: BookmarkFolderElement;
  let bookmarksApi: TestBookmarksApiProxy;

  const folder: chrome.bookmarks.BookmarkTreeNode = {
    id: '0',
    title: 'Bookmarks bar',
    children: [
      {
        id: '1',
        title: 'Shopping list',
        children: [
          {
            id: '4',
            title: 'New shoes',
            url: 'http://shoes/',
          },
        ],
      },
      {
        id: '2',
        title: 'Foo website',
        url: 'http://foo/',
      },
      {
        id: '3',
        title: 'Bar website',
        url: 'http://bar/',
      },
    ],
  };

  function getChildElements(): Array<HTMLElement|BookmarkFolderElement> {
    return Array.from(bookmarkFolder.shadowRoot!.querySelectorAll(
        'bookmark-folder, .bookmark'));
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    bookmarksApi = new TestBookmarksApiProxy();
    BookmarksApiProxyImpl.setInstance(bookmarksApi);

    bookmarkFolder = document.createElement('bookmark-folder');
    bookmarkFolder.folder = folder;
    bookmarkFolder.openFolders = ['0'];
    document.body.appendChild(bookmarkFolder);

    await flushTasks();
  });

  test('UpdatesDepthVariables', () => {
    bookmarkFolder.depth = 3;
    assertEquals('3', bookmarkFolder.style.getPropertyValue('--node-depth'));
    assertEquals('4', bookmarkFolder.style.getPropertyValue('--child-depth'));
  });

  test('RendersChildren', () => {
    const childElements = getChildElements();
    assertEquals(3, childElements.length);

    assertTrue(childElements[0] instanceof BookmarkFolderElement);
    assertEquals(
        folder.children![0],
        (childElements[0]! as BookmarkFolderElement).folder);

    assertEquals(
        folder.children![1]!.title,
        childElements[1]!.querySelector('.title')!.textContent);
    assertEquals(
        folder.children![2]!.title,
        childElements[2]!.querySelector('.title')!.textContent);
  });

  test('UpdatesChildCountVariable', () => {
    assertEquals('3', bookmarkFolder.style.getPropertyValue('--child-count'));

    bookmarkFolder.folder = Object.assign({}, folder, {
      children: [
        {
          id: '1',
          title: 'Shopping list',
          children: [],
        },
      ],
    });
    assertEquals('1', bookmarkFolder.style.getPropertyValue('--child-count'));

    bookmarkFolder.folder = Object.assign({}, folder, {children: undefined});
    assertEquals('0', bookmarkFolder.style.getPropertyValue('--child-count'));
  });

  test('ShowsFaviconForBookmarks', () => {
    const fooWebsiteElement = getChildElements()[1]!;
    assertEquals(
        getFaviconForPageURL(folder.children![1]!.url!, false),
        fooWebsiteElement.querySelector<HTMLElement>('.icon')!.style
            .getPropertyValue('background-image'));
  });

  test('OpensAndClosesFolder', async () => {
    const arrowIcon =
        bookmarkFolder.shadowRoot!.querySelector<HTMLElement>('#arrowIcon')!;
    assertTrue(arrowIcon.hasAttribute('open'));
    assertEquals(3, getChildElements().length);

    const eventPromise =
        eventToPromise(FOLDER_OPEN_CHANGED_EVENT, document.body);
    bookmarkFolder.shadowRoot!.querySelector<HTMLElement>('.row')!.click();
    await eventPromise;

    // Normally, the event listener for FOLDER_OPEN_CHANGED_EVENT will update
    // the openFolders property.
    bookmarkFolder.openFolders = [];
    await waitAfterNextRender(bookmarkFolder);
    assertFalse(arrowIcon.hasAttribute('open'));
    assertEquals(0, getChildElements().length);
  });

  test('UpdatesOpenStateBasedOnOpenFolders', async () => {
    bookmarkFolder.openFolders = [];
    await waitAfterNextRender(bookmarkFolder);
    getChildElements().forEach(
        child => assertEquals('none', child.style.display));
  });

  test('OpensBookmark', async () => {
    getChildElements()[1]!.click();
    const [id, parentFolderDepth, , source] =
        await bookmarksApi.whenCalled('openBookmark');
    assertEquals(folder.children![1]!.id, id);
    assertEquals(0, parentFolderDepth);
    assertEquals(ActionSource.kBookmark, source);
  });

  test('OpensBookmarkContextMenu', async () => {
    getChildElements()[1]!.dispatchEvent(new MouseEvent('contextmenu'));
    const [id, , , source] = await bookmarksApi.whenCalled('showContextMenu');
    assertEquals(folder.children![1]!.id, id);
    assertEquals(ActionSource.kBookmark, source);
  });

  test('MovesFocusDown', () => {
    // No focus yet, should focus folder row.
    assertTrue(bookmarkFolder.moveFocus(1));
    assertEquals(
        bookmarkFolder.shadowRoot!.querySelector('.row'),
        bookmarkFolder.shadowRoot!.activeElement);

    // Move focus down one, should focus first child which is a folder.
    assertTrue(bookmarkFolder.moveFocus(1));
    assertEquals(
        bookmarkFolder.shadowRoot!.querySelector('#children bookmark-folder'),
        bookmarkFolder.shadowRoot!.activeElement);

    const bookmarkElements =
        bookmarkFolder.shadowRoot!.querySelectorAll('#children .row');
    // Move focus down one, should focus second child, the first bookmark.
    assertTrue(bookmarkFolder.moveFocus(1));
    assertEquals(bookmarkElements[0], bookmarkFolder.shadowRoot!.activeElement);

    // Move focus down one, should focus second child, the second bookmark.
    assertTrue(bookmarkFolder.moveFocus(1));
    assertEquals(bookmarkElements[1], bookmarkFolder.shadowRoot!.activeElement);

    // No more room.
    assertFalse(bookmarkFolder.moveFocus(1));
  });

  test('MovesFocusUp', () => {
    // No focus yet, should focus last bookmark.
    const bookmarkElements =
        bookmarkFolder.shadowRoot!.querySelectorAll('#children .row');
    assertTrue(bookmarkFolder.moveFocus(-1));
    assertEquals(
        bookmarkElements[bookmarkElements.length - 1],
        bookmarkFolder.shadowRoot!.activeElement);

    // Move focus up one, should focus the first bookmark.
    assertTrue(bookmarkFolder.moveFocus(-1));
    assertEquals(bookmarkElements[0], bookmarkFolder.shadowRoot!.activeElement);

    // Move focus up one, should focus the child folder.
    assertTrue(bookmarkFolder.moveFocus(-1));
    assertEquals(
        bookmarkFolder.shadowRoot!.querySelector('#children bookmark-folder'),
        bookmarkFolder.shadowRoot!.activeElement);

    // Move focus up one, should focus the folder itself.
    assertTrue(bookmarkFolder.moveFocus(-1));
    assertEquals(
        bookmarkFolder.shadowRoot!.querySelector('.row'),
        bookmarkFolder.shadowRoot!.activeElement);

    // No more room.
    assertFalse(bookmarkFolder.moveFocus(-1));
  });

  test('DoesNotFocusHiddenChildren', async () => {
    bookmarkFolder.openFolders = [];
    await waitAfterNextRender(bookmarkFolder);
    assertTrue(bookmarkFolder.moveFocus(1));   // Moves focus to folder.
    assertFalse(bookmarkFolder.moveFocus(1));  // No children to move focus to.
  });

  test('MovesFocusWithinNestedFolders', async () => {
    bookmarkFolder.folder = {
      id: '0',
      title: 'Bookmarks bar',
      children: [{
        id: '1',
        title: 'Nested folder 1',
        children: [{
          id: '2',
          title: 'Nested folder 2',
          children: [{
            id: '3',
            title: 'Nested folder 3',
            children: [],
          }],
        }],
      }],
    };
    bookmarkFolder.openFolders = ['0', '1', '2', '3'];
    await waitAfterNextRender(bookmarkFolder);

    // Move focus down 1, should focus root folder.
    assertTrue(bookmarkFolder.moveFocus(1));
    assertEquals(
        bookmarkFolder.shadowRoot!.querySelector('.row'),
        bookmarkFolder.shadowRoot!.activeElement);

    // Move focus down 1, should focus first nested folder.
    assertTrue(bookmarkFolder.moveFocus(1));
    assertEquals(
        bookmarkFolder.folder.children![0],
        (bookmarkFolder.shadowRoot!.activeElement! as BookmarkFolderElement)
            .folder);

    // Move focus down 1, should focus grandchild folder.
    assertTrue(bookmarkFolder.moveFocus(1));
    assertEquals(
        bookmarkFolder.folder.children![0]!.children![0],
        (bookmarkFolder.shadowRoot!.activeElement!.shadowRoot!.activeElement! as
         BookmarkFolderElement)
            .folder);

    // Move focus down 1, should focus great grandchild folder.
    assertTrue(bookmarkFolder.moveFocus(1));
    assertEquals(
        bookmarkFolder.folder.children![0]!.children![0]!.children![0],
        (bookmarkFolder.shadowRoot!.activeElement!.shadowRoot!.activeElement!
             .shadowRoot!.activeElement! as BookmarkFolderElement)
            .folder);

    // Move focus up 1, should focus grandchild folder.
    assertTrue(bookmarkFolder.moveFocus(-1));
    assertEquals(
        bookmarkFolder.folder.children![0]!.children![0],
        (bookmarkFolder.shadowRoot!.activeElement!.shadowRoot!.activeElement! as
         BookmarkFolderElement)
            .folder);
  });

  test('SendsClickModifiers', async () => {
    const item = getChildElements()[1]!;
    item.dispatchEvent(new MouseEvent('click'));
    const [, , click] = await bookmarksApi.whenCalled('openBookmark');
    assertFalse(
        click.middleButton || click.altKey || click.ctrlKey || click.metaKey ||
        click.shiftKey);
    bookmarksApi.resetResolver('openBookmark');

    // Middle mouse button click.
    item.dispatchEvent(new MouseEvent('auxclick', {button: 1}));
    const [, , auxClick] = await bookmarksApi.whenCalled('openBookmark');
    assertTrue(auxClick.middleButton);
    assertFalse(
        auxClick.altKey || auxClick.ctrlKey || auxClick.metaKey ||
        auxClick.shiftKey);
    bookmarksApi.resetResolver('openBookmark');

    // Non-middle mouse aux clicks.
    item.dispatchEvent(new MouseEvent('auxclick', {button: 2}));
    assertEquals(0, bookmarksApi.getCallCount('openBookmark'));

    // Modifier keys.
    item.dispatchEvent(new MouseEvent('click', {
      altKey: true,
      ctrlKey: true,
      metaKey: true,
      shiftKey: true,
    }));
    const [, , modifiedClick] = await bookmarksApi.whenCalled('openBookmark');
    assertFalse(modifiedClick.middleButton);
    assertTrue(
        modifiedClick.altKey && modifiedClick.ctrlKey &&
        modifiedClick.metaKey && modifiedClick.shiftKey);
  });

  test('GetsFocusableElements', async () => {
    let focusableElement = bookmarkFolder.getFocusableElement([folder]);
    assertTrue(!!focusableElement);
    assertEquals('folder', focusableElement!.id);

    const childBookmark = folder.children![1]!;
    focusableElement = bookmarkFolder.getFocusableElement([childBookmark]);
    assertTrue(!!focusableElement);
    assertTrue(focusableElement!.classList.contains('bookmark'));
    assertEquals(childBookmark, getBookmarkFromElement(focusableElement!));

    const childFolder = folder.children![0]!;
    focusableElement = bookmarkFolder.getFocusableElement([childFolder]);
    assertTrue(!!focusableElement);
    assertEquals('folder', focusableElement!.id);
    assertEquals(childFolder.id, getBookmarkFromElement(focusableElement!).id);

    // Grandchild bookmark is in a closed folder, so the focusable element
    // should still be the child folder.
    const grandchildBookmark = childFolder.children![0]!;
    focusableElement =
        bookmarkFolder.getFocusableElement([childFolder, grandchildBookmark]);
    assertTrue(!!focusableElement);
    assertEquals('folder', focusableElement!.id);
    assertEquals(childFolder.id, getBookmarkFromElement(focusableElement!).id);

    // Once the child folder is opened, the grandchild bookmark element should
    // be focusable.
    bookmarkFolder.openFolders = ['0', '1'];
    await waitAfterNextRender(bookmarkFolder);
    focusableElement =
        bookmarkFolder.getFocusableElement([childFolder, grandchildBookmark]);
    assertTrue(!!focusableElement);
    assertTrue(focusableElement!.classList.contains('bookmark'));
    assertEquals(grandchildBookmark, getBookmarkFromElement(focusableElement!));
  });
});

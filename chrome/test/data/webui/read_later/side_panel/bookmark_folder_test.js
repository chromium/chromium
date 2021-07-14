// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ReadLaterUI is a Mojo WebUI controller and therefore needs mojo defined to
// finish running its tests.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {BookmarkFolderElement, FOLDER_OPEN_CHANGED_EVENT} from 'chrome://read-later.top-chrome/side_panel/bookmark_folder.js';
import {BookmarksApiProxy} from 'chrome://read-later.top-chrome/side_panel/bookmarks_api_proxy.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.m.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {eventToPromise, flushTasks, waitAfterNextRender} from '../../test_util.m.js';

import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';

suite('SidePanelBookmarkFolderTest', () => {
  /** @type {!BookmarkFolderElement} */
  let bookmarkFolder;

  /** @type {!TestBookmarksApiProxy} */
  let bookmarksApi;

  /** @type {!chrome.bookmarks.BookmarkTreeNode} */
  const folder = {
    id: '0',
    title: 'Bookmarks bar',
    children: [
      {
        id: '1',
        title: 'Shopping list',
        children: [],
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

  /** @return {!Array<!HTMLElement|!BookmarkFolderElement>} */
  function getChildElements() {
    return /** @type {!Array<!HTMLElement|!BookmarkFolderElement>} */ (
        Array.from(bookmarkFolder.shadowRoot.querySelectorAll(
            'bookmark-folder, .bookmark')));
  }

  setup(async () => {
    document.body.innerHTML = '';

    bookmarksApi = new TestBookmarksApiProxy();
    BookmarksApiProxy.setInstance(bookmarksApi);

    bookmarkFolder = /** @type {!BookmarkFolderElement} */ (
        document.createElement('bookmark-folder'));
    bookmarkFolder.folder = folder;
    bookmarkFolder.openFolders = ['0'];
    document.body.appendChild(bookmarkFolder);

    await flushTasks();
  });

  test('UpdatesDepthVariables', () => {
    bookmarkFolder.depth = 3;
    assertEquals('3', bookmarkFolder.style.getPropertyValue('--node-depth'));
    assertEquals(
        '4',
        bookmarkFolder.shadowRoot.querySelector('#children')
            .style.getPropertyValue('--node-depth'));
  });

  test('RendersChildren', () => {
    const childElements = getChildElements();
    assertEquals(3, childElements.length);

    assertTrue(childElements[0] instanceof BookmarkFolderElement);
    assertEquals(folder.children[0], childElements[0].folder);

    assertEquals(
        folder.children[1].title,
        childElements[1].querySelector('.title').innerText);
    assertEquals(
        folder.children[2].title,
        childElements[2].querySelector('.title').innerText);
  });

  test('ShowsFaviconForBookmarks', () => {
    const fooWebsiteElement = getChildElements()[1];
    assertEquals(
        getFaviconForPageURL(folder.children[1].url, false),
        fooWebsiteElement.querySelector('.icon').style.getPropertyValue(
            'background-image'));
  });

  test('OpensAndClosesFolder', async () => {
    const arrowIcon = bookmarkFolder.shadowRoot.querySelector('#arrowIcon');
    assertTrue(arrowIcon.hasAttribute('open'));
    assertEquals(3, getChildElements().length);

    const eventPromise =
        eventToPromise(FOLDER_OPEN_CHANGED_EVENT, document.body);
    bookmarkFolder.shadowRoot.querySelector('.row').click();
    assertFalse(arrowIcon.hasAttribute('open'));
    assertEquals(3, getChildElements().length);
    await eventPromise;
  });

  test('UpdatesOpenStateBasedOnOpenFolders', async () => {
    bookmarkFolder.openFolders = [];
    await waitAfterNextRender();
    getChildElements().forEach(
        child => assertEquals('none', child.style.display));
  });

  test('OpensBookmark', async () => {
    getChildElements()[1].click();
    const [url, parentFolderDepth] =
        await bookmarksApi.whenCalled('openBookmark');
    assertEquals(folder.children[1].url, url);
    assertEquals(0, parentFolderDepth);
  });

  test('MovesFocusDown', () => {
    // No focus yet, should focus folder row.
    assertTrue(bookmarkFolder.moveFocus(1));
    assertEquals(
        bookmarkFolder.shadowRoot.querySelector('.row'),
        bookmarkFolder.shadowRoot.activeElement);

    // Move focus down one, should focus first child which is a folder.
    assertTrue(bookmarkFolder.moveFocus(1));
    assertEquals(
        bookmarkFolder.shadowRoot.querySelector('#children bookmark-folder'),
        bookmarkFolder.shadowRoot.activeElement);

    const bookmarkElements =
        bookmarkFolder.shadowRoot.querySelectorAll('#children .row');
    // Move focus down one, should focus second child, the first bookmark.
    assertTrue(bookmarkFolder.moveFocus(1));
    assertEquals(bookmarkElements[0], bookmarkFolder.shadowRoot.activeElement);

    // Move focus down one, should focus second child, the second bookmark.
    assertTrue(bookmarkFolder.moveFocus(1));
    assertEquals(bookmarkElements[1], bookmarkFolder.shadowRoot.activeElement);

    // No more room.
    assertFalse(bookmarkFolder.moveFocus(1));
  });

  test('MovesFocusUp', () => {
    // No focus yet, should focus last bookmark.
    const bookmarkElements =
        bookmarkFolder.shadowRoot.querySelectorAll('#children .row');
    assertTrue(bookmarkFolder.moveFocus(-1));
    assertEquals(
        bookmarkElements[bookmarkElements.length - 1],
        bookmarkFolder.shadowRoot.activeElement);

    // Move focus up one, should focus the first bookmark.
    assertTrue(bookmarkFolder.moveFocus(-1));
    assertEquals(bookmarkElements[0], bookmarkFolder.shadowRoot.activeElement);

    // Move focus up one, should focus the child folder.
    assertTrue(bookmarkFolder.moveFocus(-1));
    assertEquals(
        bookmarkFolder.shadowRoot.querySelector('#children bookmark-folder'),
        bookmarkFolder.shadowRoot.activeElement);

    // Move focus up one, should focus the folder itself.
    assertTrue(bookmarkFolder.moveFocus(-1));
    assertEquals(
        bookmarkFolder.shadowRoot.querySelector('.row'),
        bookmarkFolder.shadowRoot.activeElement);

    // No more room.
    assertFalse(bookmarkFolder.moveFocus(-1));
  });

  test('DoesNotFocusHiddenChildren', async () => {
    bookmarkFolder.openFolders = [];
    await waitAfterNextRender();
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
    await waitAfterNextRender();

    // Move focus down 1, should focus root folder.
    assertTrue(bookmarkFolder.moveFocus(1));
    assertEquals(
        bookmarkFolder.shadowRoot.querySelector('.row'),
        bookmarkFolder.shadowRoot.activeElement);

    // Move focus down 1, should focus first nested folder.
    assertTrue(bookmarkFolder.moveFocus(1));
    assertEquals(
        bookmarkFolder.folder.children[0],
        bookmarkFolder.shadowRoot.activeElement.folder);

    // Move focus down 1, should focus grandchild folder.
    assertTrue(bookmarkFolder.moveFocus(1));
    assertEquals(
        bookmarkFolder.folder.children[0].children[0],
        bookmarkFolder.shadowRoot.activeElement.shadowRoot.activeElement
            .folder);

    // Move focus down 1, should focus great grandchild folder.
    assertTrue(bookmarkFolder.moveFocus(1));
    assertEquals(
        bookmarkFolder.folder.children[0].children[0].children[0],
        bookmarkFolder.shadowRoot.activeElement.shadowRoot.activeElement
            .shadowRoot.activeElement.folder);

    // Move focus up 1, should focus grandchild folder.
    assertTrue(bookmarkFolder.moveFocus(-1));
    assertEquals(
        bookmarkFolder.folder.children[0].children[0],
        bookmarkFolder.shadowRoot.activeElement.shadowRoot.activeElement
            .folder);
  });
});
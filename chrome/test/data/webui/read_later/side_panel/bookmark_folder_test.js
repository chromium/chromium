// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ReadLaterUI is a Mojo WebUI controller and therefore needs mojo defined to
// finish running its tests.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {ReadLaterApiProxy, ReadLaterApiProxyImpl} from 'chrome://read-later.top-chrome/read_later_api_proxy.js';
import {BookmarkFolderElement} from 'chrome://read-later.top-chrome/side_panel/bookmark_folder.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.m.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';
import {TestReadLaterApiProxy} from '../test_read_later_api_proxy.js';

suite('SidePanelBookmarkFolderTest', () => {
  /** @type {!BookmarkFolderElement} */
  let bookmarkFolder;

  /** @type {!TestReadLaterApiProxy} */
  let readLaterApi;

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

    readLaterApi = new TestReadLaterApiProxy();
    ReadLaterApiProxyImpl.instance_ = readLaterApi;

    bookmarkFolder = /** @type {!BookmarkFolderElement} */ (
        document.createElement('bookmark-folder'));
    bookmarkFolder.folder = folder;
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

  test('OpensAndClosesFolder', () => {
    const arrowIcon = bookmarkFolder.shadowRoot.querySelector('#arrowIcon');
    const childrenElement =
        bookmarkFolder.shadowRoot.querySelector('#children');
    assertFalse(arrowIcon.hasAttribute('open'));
    assertTrue(childrenElement.hasAttribute('hidden'));

    bookmarkFolder.shadowRoot.querySelector('.row').click();
    assertTrue(arrowIcon.hasAttribute('open'));
    assertFalse(childrenElement.hasAttribute('hidden'));
  });

  test('OpensBookmark', async () => {
    getChildElements()[1].click();
    const [url, updateReadStatus] = await readLaterApi.whenCalled('openURL');
    assertEquals(folder.children[1].url, url.url);
    assertFalse(updateReadStatus);
  });
});
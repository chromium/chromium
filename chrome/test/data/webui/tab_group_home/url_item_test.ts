// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tab-group-home/url_item_grid/url_item.js';

import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import type {UrlItemElement} from 'chrome://tab-group-home/url_item_grid/url_item.js';
import type {UrlItem} from 'chrome://tab-group-home/url_item_grid/url_item_delegate.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('UrlItemElementTest', () => {
  let urlItemElement: UrlItemElement;

  const sampleItem: UrlItem = {
    id: 123,
    title: 'Test Title',
    url: {url: 'https://www.example.com'},
  };

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    urlItemElement = document.createElement('url-item');
    urlItemElement.item = sampleItem;
    document.body.appendChild(urlItemElement);
    return microtasksFinished();
  });

  test('renders item data correctly', () => {
    const titleElement = urlItemElement.$.title;
    assertEquals(sampleItem.title, titleElement.textContent.trim());

    const faviconElement = urlItemElement.$.favicon;
    assertEquals(
        getFaviconForPageURL(sampleItem.url.url, false),
        faviconElement.style.backgroundImage);
  });

  test('close button click fires', async () => {
    const eventPromise = eventToPromise('close-button-click', urlItemElement);

    const closeButton = urlItemElement.$.closeButton;
    closeButton.click();

    const event = await eventPromise;
    assertEquals(sampleItem.id, event.detail);
  });
});

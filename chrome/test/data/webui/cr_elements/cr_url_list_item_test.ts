// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';

import {CrUrlListItemElement} from 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('CrUrlListItemTest', () => {
  let element: CrUrlListItemElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('cr-url-list-item');
    document.body.appendChild(element);
  });

  test('TogglesBetweenIcons', () => {
    const favicon = element.shadowRoot!.querySelector<HTMLElement>('.favicon')!;
    const folderIcon =
        element.shadowRoot!.querySelector<HTMLElement>('.folder')!;

    element.count = 4;
    assertTrue(favicon.hasAttribute('hidden'));
    assertFalse(folderIcon.hasAttribute('hidden'));

    element.count = undefined;
    element.url = 'http://google.com';
    assertFalse(favicon.hasAttribute('hidden'));
    assertEquals(
        getFaviconForPageURL('http://google.com', false),
        favicon.style.backgroundImage);
    assertTrue(folderIcon.hasAttribute('hidden'));
  });
});

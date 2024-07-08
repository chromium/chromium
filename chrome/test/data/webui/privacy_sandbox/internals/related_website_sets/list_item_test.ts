// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';

import type {RelatedWebsiteSetListItemElement} from 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {set1} from './test_data.js';

suite('ListItemTest', () => {
  let item: RelatedWebsiteSetListItemElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    item = document.createElement('related-website-set-list-item');
    document.body.appendChild(item);
    item.setItemForTesting(set1);
    await microtasksFinished();
  });

  test('check layout', async () => {
    assertTrue(isVisible(item));
    assertFalse(item.$.expandedContent.opened);
  });

  test('check expansion', async () => {
    item.$.expandButton.click();
    await microtasksFinished();

    assertTrue(item.$.expandedContent.opened);
    const memberSites = Array.from(item.$.expandedContent.children);
    assertEquals(set1.memberSites.length, memberSites.length);

    memberSites.forEach(member => assertTrue(isVisible(member)));
  });
});

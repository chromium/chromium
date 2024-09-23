// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';

import type {RelatedWebsiteSet, RelatedWebsiteSetsListContainerElement} from 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {SAMPLE_RELATED_WEBSITE_SETS} from './test_data.js';

suite('ContainerTest', () => {
  let container: RelatedWebsiteSetsListContainerElement;
  const sampleSets: RelatedWebsiteSet[] = SAMPLE_RELATED_WEBSITE_SETS;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    container = document.createElement('related-website-sets-list-container');
    document.body.appendChild(container);
    container.relatedWebsiteSets = sampleSets;
    await microtasksFinished();
  });

  test('check layout', async () => {
    assertTrue(isVisible(container));
    const renderedItems = container.shadowRoot!.querySelectorAll(
        'related-website-sets-list-item');
    assertEquals(sampleSets.length, renderedItems.length);
  });

  test('check expand collapse', async () => {
    assertEquals(
        'Expand All', container.$.expandCollapseButton.textContent!.trim());
    const renderedItems = container.shadowRoot!.querySelectorAll(
        'related-website-sets-list-item');
    renderedItems.forEach(item => assertFalse(item.$.expandedContent.opened));
    container.$.expandCollapseButton.click();
    await microtasksFinished();

    // Verify that all items were opened when button clicked
    assertEquals(
        'Collapse All', container.$.expandCollapseButton.textContent!.trim());
    renderedItems.forEach(item => assertTrue(item.$.expandedContent.opened));
    container.$.expandCollapseButton.click();
    await microtasksFinished();

    // Verify that all items close when button clicked again
    assertEquals(
        'Expand All', container.$.expandCollapseButton.textContent!.trim());
    renderedItems.forEach(item => assertFalse(item.$.expandedContent.opened));
  });

  test('check search results', async () => {
    assertEquals(3, container.filteredItems.length);
    container.query = 'sEt2';
    await microtasksFinished();
    assertEquals(1, container.filteredItems.length);
    assertTrue(!!container.filteredItems);
    for (const item of container.filteredItems) {
      assertTrue(item.primarySite.includes(container.query.toLowerCase()));
    }
  });
});

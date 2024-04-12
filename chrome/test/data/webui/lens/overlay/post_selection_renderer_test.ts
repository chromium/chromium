// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/post_selection_renderer.js';

import type {PostSelectionBoundingBox, PostSelectionRendererElement} from 'chrome-untrusted://lens/post_selection_renderer.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome-untrusted://webui-test/test_util.js';

const TEST_WIDTH = 800;
const TEST_HEIGHT = 500;
const CORNER_WIDTH = 4;

suite('PostSelectionRenderer', () => {
  let postSelectionRenderer: PostSelectionRendererElement;

  setup(() => {
    // Resetting the HTML needs to be the first thing we do in setup to
    // guarantee that any singleton instances don't change while any UI is still
    // attached to the DOM.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    postSelectionRenderer = document.createElement('post-selection-renderer');

    postSelectionRenderer.style.display = 'block';
    postSelectionRenderer.style.width = `${TEST_WIDTH}px`;
    postSelectionRenderer.style.height = `${TEST_HEIGHT}px`;

    document.body.appendChild(postSelectionRenderer);
    return waitAfterNextRender(postSelectionRenderer);
  });

  async function triggerPostSelectionRender(
      boundingBox: PostSelectionBoundingBox): Promise<void> {
    document.dispatchEvent(
        new CustomEvent('render-post-selection', {detail: boundingBox}));
    return waitAfterNextRender(postSelectionRenderer);
  }

  test('PostSelectionUnhides', async () => {
    // Verifies the post selection bounding box unhides after receiving
    // `render-post-selection` event.
    await triggerPostSelectionRender(
        {top: 0, left: 0, width: 0.2, height: 0.2});

    assertTrue(isVisible(postSelectionRenderer.$.selectionCorners));
  });

  test('PostSelectionRendersCorrectly', async () => {
    // Verifies the post selection bounding box renders correctly after
    // receiving `render-post-selection` event.
    const postSelectionBox = {top: 0.2, left: 0.35, width: 0.5, height: 0.3};
    await triggerPostSelectionRender(postSelectionBox);

    const renderedSelectionBox =
        postSelectionRenderer.$.selectionCorners.getBoundingClientRect();
    assertEquals(
        TEST_HEIGHT * postSelectionBox.top - CORNER_WIDTH,
        renderedSelectionBox.top);
    assertEquals(
        TEST_WIDTH * postSelectionBox.left - CORNER_WIDTH,
        renderedSelectionBox.left);
    assertEquals(
        TEST_HEIGHT * postSelectionBox.height + (2 * CORNER_WIDTH),
        renderedSelectionBox.height);
    assertEquals(
        TEST_WIDTH * postSelectionBox.width + (2 * CORNER_WIDTH),
        renderedSelectionBox.width);
  });

  test('PostSelectionHides', async () => {
    // Verifies the post selection bounding box hides after receiving
    // `render-post-selection` event with invalid dimensions.
    await triggerPostSelectionRender(
        {top: 0, left: 100, height: -1, width: 50});

    assertFalse(isVisible(postSelectionRenderer.$.selectionCorners));
  });

  test('PostSelectionClearSelection', async () => {
    // Verifies the post selection bounding box hides after clearSelection() is
    // called.
    await triggerPostSelectionRender(
        {top: 0, left: 0, width: 0.2, height: 0.2});
    assertTrue(isVisible(postSelectionRenderer.$.selectionCorners));

    postSelectionRenderer.clearSelection();
    await waitAfterNextRender(postSelectionRenderer);
    assertFalse(isVisible(postSelectionRenderer.$.selectionCorners));
  });
});

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/loading_state.js';

import type {LoadingStateElement} from 'chrome://compare/loading_state.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('LoadingStateTest', () => {
  let loadingElement: LoadingStateElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadingElement = document.createElement('loading-state');
    document.body.appendChild(loadingElement);
  });

  function resizeContainer(width: number): Promise<void> {
    // A ResizeObserver is used to ensure the ResizeObserver callback that shows
    // the last column gradient has completed.
    return new Promise<void>(resolve => {
      const observer = new ResizeObserver(() => {
        resolve();
        observer.unobserve(loadingElement.$.loadingContainer);
      });
      observer.observe(loadingElement.$.loadingContainer);
      loadingElement.$.loadingContainer.style.width = `${width}px`;
    });
  }

  test(
      'last column gradient appears when SVG is wider than its container',
      async () => {
        const lastColumnGradient =
            $$<HTMLElement>(loadingElement, '#lastColumnGradient');
        assertTrue(!!lastColumnGradient);

        // Container should fit loading gradient without overflow.
        loadingElement.columnCount = 3;
        await microtasksFinished();
        assertFalse(isVisible(lastColumnGradient));

        // Container should be smaller than the loading gradient, so the last
        // column gradient should appear.
        await resizeContainer(200);
        await microtasksFinished();
        assertTrue(isVisible(lastColumnGradient));
      });
});

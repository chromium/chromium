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
        document.body.style.width = '200px';
        await microtasksFinished();
        assertTrue(isVisible(lastColumnGradient));
      });
});

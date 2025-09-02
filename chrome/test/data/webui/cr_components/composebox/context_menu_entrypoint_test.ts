// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ContextMenuEntrypointElement} from 'chrome://resources/cr_components/composebox/context_menu_entrypoint.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('ContextMenuEntrypoint', () => {
  let entrypoint: ContextMenuEntrypointElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    entrypoint = new ContextMenuEntrypointElement();
    document.body.appendChild(entrypoint);
    await microtasksFinished();
  });

  test('clicking entrypoint shows context menu', async () => {
    // Act.
    entrypoint.$.entrypoint.click();
    await microtasksFinished();

    // Assert.
    assertTrue(entrypoint.$.menu.open);
  });

  ['#fileUpload', '#imageUpload'].forEach((selector) => {
    test(`clicking ${selector} hides context menu`, async () => {
      // Arrange.
      entrypoint.$.entrypoint.click();
      await microtasksFinished();
      assertTrue(entrypoint.$.menu.open);

      // Act.
      const button = $$(entrypoint, selector);
      assertTrue(!!button);
      button.click();
      await microtasksFinished();

      // Assert.
      assertFalse(entrypoint.$.menu.open);
    });
  });
});

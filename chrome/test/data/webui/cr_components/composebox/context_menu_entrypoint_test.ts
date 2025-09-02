// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ContextMenuEntrypointElement} from 'chrome://resources/cr_components/composebox/context_menu_entrypoint.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

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

  ([
    ['#fileUpload', 'open-file-upload'],
    ['#imageUpload', 'open-image-upload'],
  ] as Array<[string, string]>)
      .forEach(([selector, eventName]) => {
        test(
            `clicking ${selector} propagates ${eventName} before closing menu`,
            async () => {
              // Arrange.
              entrypoint.$.entrypoint.click();
              await microtasksFinished();
              assertTrue(entrypoint.$.menu.open);

              // Act.
              const eventFired = eventToPromise(eventName, entrypoint);
              const button = $$(entrypoint, selector);
              assertTrue(!!button);
              button.click();
              await eventFired;

              // Assert.
              assertTrue(!!eventFired);
              assertFalse(entrypoint.$.menu.open);
            });
      });
});

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/composebox/contextual_entrypoint_button.js';

import type {ContextualEntrypointButtonElement} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_button.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('ContextualEntrypointButton', () => {
  let entrypointButton: ContextualEntrypointButtonElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    entrypointButton =
        document.createElement('cr-composebox-contextual-entrypoint-button');
    document.body.appendChild(entrypointButton);
    await microtasksFinished();
  });

  test('clicking entrypoint fires event', async () => {
    // Act.
    const eventPromise =
        eventToPromise('context-menu-entrypoint-click', entrypointButton);
    $$(entrypointButton, '#entrypoint')!.click();
    await microtasksFinished();

    // Assert.
    const event = await eventPromise;
    assertTrue(!!event);
  });
});

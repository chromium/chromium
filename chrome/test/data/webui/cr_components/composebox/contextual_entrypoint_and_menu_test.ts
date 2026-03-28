// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/composebox/contextual_entrypoint_and_menu.js';

import type {ContextualEntrypointAndMenuElement} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {InputState} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createValidInputState} from './composebox_test_utils.js';

suite('ContextualEntrypointAndMenu', () => {
  let entrypointAndMenu: ContextualEntrypointAndMenuElement;

  async function createComponent(
      inputState: InputState|null = createValidInputState()) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    entrypointAndMenu =
        document.createElement('cr-composebox-contextual-entrypoint-and-menu');
    Object.assign(entrypointAndMenu, {
      inputState,
      showModelPicker: true,
    });
    document.body.appendChild(entrypointAndMenu);
    await microtasksFinished();
  }

  setup(async () => {
    loadTimeData.overrideValues({
      composeboxContextMenuEnableMultiTabSelection: true,
      contextualMenuUsePecApi: true,
    });
    await createComponent();
  });

  test('context menu shown on entrypoint click event', async () => {
    // Act.
    $$(entrypointAndMenu, '#entrypointButton')!.dispatchEvent(
        new Event('context-menu-entrypoint-click'));
    await microtasksFinished();

    // Assert.
    assertTrue(entrypointAndMenu.$.menu.open);
  });

  test('entrypoint button is displayed when not using pec api', async () => {
    loadTimeData.overrideValues({
      contextualMenuUsePecApi: false,
    });
    await createComponent(/* inputState= */ null);

    const entrypointButton = $$(entrypointAndMenu, '#entrypointButton');

    assertTrue(!!entrypointButton);
  });

  test('entrypoint button is displayed with valid input state', () => {
    const entrypoint = $$(entrypointAndMenu, '#entrypointButton');

    assertTrue(!!entrypoint);
  });

  test('entrypoint button is hidden with invalid input state', async () => {
    entrypointAndMenu.inputState = null;
    await microtasksFinished();

    const entrypoint = $$(entrypointAndMenu, '#entrypointButton');

    assertFalse(!!entrypoint);
  });

  test('event fired on context menu open and close', async () => {
    // Act open menu.
    const openEventPromise =
        eventToPromise('context-menu-opened', entrypointAndMenu);
    entrypointAndMenu.openMenuForMultiSelection();
    await microtasksFinished();

    const entrypointButton = $$(entrypointAndMenu, '#entrypointButton');
    assertTrue(!!entrypointButton);

    // Assert open menu.
    assertTrue(entrypointAndMenu.$.menu.open);
    assertTrue(!!(await openEventPromise));
    assertTrue(entrypointButton.classList.contains('menu-open'));

    // Act close menu.
    const closeEventPromise =
        eventToPromise('context-menu-closed', entrypointAndMenu);
    entrypointAndMenu.closeMenu();
    await microtasksFinished();

    // Assert close menu.
    assertTrue(!!(await closeEventPromise));
    assertFalse(entrypointButton.classList.contains('menu-open'));
  });

  test(
      'context menu does not open if multi-tab selection is disabled',
      async () => {
        loadTimeData.overrideValues({
          composeboxContextMenuEnableMultiTabSelection: false,
        });
        await createComponent();

        entrypointAndMenu.openMenuForMultiSelection();
        await microtasksFinished();

        assertFalse(entrypointAndMenu.$.menu.open);
      });
});

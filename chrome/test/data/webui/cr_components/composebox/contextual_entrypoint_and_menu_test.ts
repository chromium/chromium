// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/composebox/contextual_entrypoint_and_menu.js';

import type {ContextualEntrypointAndMenuElement} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {InputType, ModelMode, ToolMode} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('ContextualEntrypointAndMenu', () => {
  let entrypointAndMenu: ContextualEntrypointAndMenuElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      composeboxContextMenuEnableMultiTabSelection: true,
    });

    entrypointAndMenu =
        document.createElement('cr-composebox-contextual-entrypoint-and-menu');
    Object.assign(entrypointAndMenu, {
      inputState: {
        allowedModels: [ModelMode.kGeminiPro],
        allowedTools: [ToolMode.kDeepSearch],
        allowedInputTypes: [InputType.kBrowserTab],
        activeModel: ModelMode.kUnspecified,
        activeTool: ToolMode.kUnspecified,
        disabledModels: [],
        disabledTools: [],
        disabledInputTypes: [],
        toolConfigs: [
          {
            tool: ToolMode.kDeepSearch,
            menuLabel: 'Deep Search',
            chipLabel: 'Deep Search',
            hintText: 'Deep Search hint',
            disableActiveModelSelection: false,
          },
        ],
        modelConfigs: [
          {
            model: ModelMode.kGeminiPro,
            menuLabel: 'Gemini Pro',
            hintText: 'Gemini Pro hint',
          },
        ],
        modelSectionConfig: {
          header: 'Models',
        },
      },
      showModelPicker: true,
    });
    document.body.appendChild(entrypointAndMenu);
    await microtasksFinished();
  });

  test('context menu shown on entrypoint click event', async () => {
    // Act.
    $$(entrypointAndMenu, '#entrypointButton')!.dispatchEvent(
        new Event('context-menu-entrypoint-click'));
    await microtasksFinished();

    // Assert.
    assertTrue(entrypointAndMenu.$.menu.open);
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
});

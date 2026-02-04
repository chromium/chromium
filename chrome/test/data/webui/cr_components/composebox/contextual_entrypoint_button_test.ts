// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/composebox/contextual_entrypoint_button.js';

import type {ContextualEntrypointButtonElement} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_button.js';
import {InputType, ModelMode, ToolMode} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('ContextualEntrypointButton', () => {
  let entrypointButton: ContextualEntrypointButtonElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    entrypointButton =
        document.createElement('cr-composebox-contextual-entrypoint-button');
    Object.assign(entrypointButton, {
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
    });
    document.body.appendChild(entrypointButton);
    await microtasksFinished();
  });

  test('clicking entrypoint shows context menu', async () => {
    // Act.
    $$(entrypointButton, '#entrypoint')!.click();
    await microtasksFinished();

    // Assert.
    assertTrue(entrypointButton.$.menu.open);
  });
});

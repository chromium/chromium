// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_components/composebox/icons.html.js';
import 'chrome://resources/cr_components/composebox/composebox_tool_chip.js';

import type {ComposeboxToolChipElement} from 'chrome://resources/cr_components/composebox/composebox_tool_chip.js';
import type {CrIconElement} from 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {ToolMode} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {MockInputState} from './composebox_test_utils.js';

suite('ComposeboxToolChipTest', () => {
  let toolChip: ComposeboxToolChipElement;

  setup(async () => {
    loadTimeData.resetForTesting({
      useSearchboxConfigIconIds: true,
      deepSearch: 'Deep Search',
      createImages: 'Create Images',
      canvas: 'Canvas',
      removeToolChipAriaLabel: 'Remove $1',
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    toolChip = document.createElement('cr-composebox-tool-chip');
    document.body.appendChild(toolChip);
    await microtasksFinished();
  });

  test('Renders icon defined in toolConfig', async () => {
    toolChip['inputState'] = new MockInputState({
      activeTool: ToolMode.kDeepSearch,
      toolConfigs: [
        {
          tool: ToolMode.kDeepSearch,
          menuLabel: 'Deep Search',
          disableActiveModelSelection: false,
          chipLabel: 'Deep Search',
          icon: 94,
          hintText: '',
          aimUrlParams: [],
          menuTooltip: '',
        },
      ],
    });
    await microtasksFinished();

    const toolIcon = $$<CrIconElement>(toolChip, '.tool-icon');
    assertTrue(isVisible(toolIcon));
    assertEquals('searchbox_config:94', toolIcon!.icon);
  });

  test(
      'Uses searchbox_config:0 when icon is 0 or unspecified in config',
      async () => {
        toolChip['inputState'] = new MockInputState({
          activeTool: ToolMode.kDeepSearch,
          toolConfigs: [
            {
              tool: ToolMode.kDeepSearch,
              menuLabel: 'Deep Search',
              disableActiveModelSelection: false,
              chipLabel: 'Deep Search',
              icon: 0,
              hintText: '',
              aimUrlParams: [],
              menuTooltip: '',
            },
          ],
        });
        await microtasksFinished();

        const toolIcon = $$<CrIconElement>(toolChip, '.tool-icon');
        assertTrue(isVisible(toolIcon));
        assertEquals('searchbox_config:0', toolIcon!.icon);
      });

  test(
      'Uses searchbox_config:0 when tool is not present in toolConfigs',
      async () => {
        toolChip['inputState'] = new MockInputState({
          activeTool: ToolMode.kDeepSearch,
          toolConfigs: [],
        });
        await microtasksFinished();

        const toolIcon = $$<CrIconElement>(toolChip, '.tool-icon');
        assertTrue(isVisible(toolIcon));
        assertEquals('searchbox_config:0', toolIcon!.icon);
      });

  test(
      'Uses legacy tool icon when useSearchboxConfigIconIds is false',
      async () => {
        loadTimeData.overrideValues({
          useSearchboxConfigIconIds: false,
        });
        toolChip['inputState'] = new MockInputState({
          activeTool: ToolMode.kDeepSearch,
          toolConfigs: [
            {
              tool: ToolMode.kDeepSearch,
              menuLabel: 'Deep Search',
              disableActiveModelSelection: false,
              chipLabel: 'Deep Search',
              icon: 94,
              hintText: '',
              aimUrlParams: [],
              menuTooltip: '',
            },
          ],
        });
        await microtasksFinished();

        const toolIcon = $$<CrIconElement>(toolChip, '.tool-icon');
        assertTrue(isVisible(toolIcon));
        assertEquals('composebox:travel-explore', toolIcon!.icon);
      });
});

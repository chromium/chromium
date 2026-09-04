// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';
import './test_composebox_mixin.js';

import {PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {ModelMode, ToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {InputState} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock, MockInputState} from './composebox_test_utils.js';
import type {TestComposeboxMixinElement} from './test_composebox_mixin.js';

suite('ComposeboxInputPlaceholder', () => {
  let composebox: TestComposeboxMixinElement;
  let searchboxHandler: TestMock<SearchboxPageHandlerRemote>;
  let windowProxy: TestMock<WindowProxy>;
  let searchboxCallbackRouter: SearchboxPageCallbackRouter;
  let searchboxPageRemote: SearchboxPageRemote;

  async function setupComposeboxWithInputState(inputState: InputState) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    searchboxHandler.setResultFor(
        'getInputState', Promise.resolve({state: inputState}));

    composebox = document.createElement('test-composebox-mixin');
    composebox.state = {
      text: '',
      files: [],
      mode: ToolMode.kUnspecified,
      model: ModelMode.kUnspecified,
      // <if expr="not is_android">
      smartTabSharingActive: false,
      // </if>
    };

    document.body.appendChild(composebox);

    searchboxPageRemote.onInputStateChanged(inputState);

    await microtasksFinished();
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    searchboxCallbackRouter = new SearchboxPageCallbackRouter();
    searchboxPageRemote = searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
    installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new SearchboxPageHandlerRemote(), searchboxCallbackRouter)));
    searchboxHandler = installMock(
        SearchboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.getInstance().searchboxHandler = mock);
    // <if expr="not is_android">
    searchboxHandler.setResultMapperFor(
        'getSmartTabSharingActive', () => Promise.resolve({active: false}));
    // </if>

    searchboxHandler.setResultFor('getRecentTabs', Promise.resolve({tabs: []}));
    searchboxHandler.setResultFor('getInputState', Promise.resolve({
      state: new MockInputState({
        toolConfigs: [],
        toolsSectionConfig: {header: ''},
        modelSectionConfig: {header: ''},
      }),
    }));

    windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('setTimeout', 0);
    windowProxy.setResultMapperFor('matchMedia', () => ({
                                                   addListener() {},
                                                   addEventListener() {},
                                                   removeListener() {},
                                                   removeEventListener() {},
                                                 }));
  });


  test('InputPlaceholderFromModelConfig', async () => {
    const modelHint = 'Ask a model';
    const testInputState = new MockInputState({
      toolConfigs: [],
      toolsSectionConfig: {header: ''},
      modelSectionConfig: {header: ''},
      activeModel: ModelMode.kGeminiRegular,
      modelConfigs: [{
        model: ModelMode.kGeminiRegular,
        hintText: modelHint,
        menuLabel: '',
        aimUrlParams: [],
        menuTooltip: '',
        icon: 0,
      }],
    });

    await setupComposeboxWithInputState(testInputState);
    assertEquals(
        modelHint,
        composebox.getInputElement().$.input.getAttribute('placeholder'));
  });

  const defaultApiHint = loadTimeData.getString('searchboxComposePlaceholder');

  const toolConfigTestCases = [
    {
      tool: ToolMode.kDeepSearch,
      hint: 'Research anything',
      name: 'DeepSearch',
    },
    {
      tool: ToolMode.kImageGen,
      hint: 'Describe your image',
      name: 'ImageGen',
    },
    {
      tool: ToolMode.kCanvas,
      hint: 'Create anything',
      name: 'Canvas',
    },
  ];

  toolConfigTestCases.forEach(({tool, hint, name}) => {
    test(`InputPlaceholderFromToolConfig_${name}`, async () => {
      await setupComposeboxWithInputState(new MockInputState({
        toolsSectionConfig: {header: ''},
        modelSectionConfig: {header: ''},
        hintText: defaultApiHint,
        toolConfigs:
            toolConfigTestCases.map(t => ({
                                      tool: t.tool,
                                      hintText: t.hint,
                                      menuLabel: '',
                                      chipLabel: '',
                                      disableActiveModelSelection: false,
                                      aimUrlParams: [],
                                      menuTooltip: '',
                                      icon: 0,
                                    })),
      }));

      // Initial placeholder check.
      assertEquals(
          defaultApiHint,
          composebox.getInputElement().$.input.getAttribute('placeholder'));

      // Enable tool mode.
      composebox.onToolClick(new CustomEvent('tool-click', {
        bubbles: true,
        composed: true,
        detail: {toolMode: tool},
      }));
      await microtasksFinished();
      searchboxPageRemote.onInputStateChanged({
        ...new MockInputState(),
        activeTool: tool,
      });
      await searchboxPageRemote.$.flushForTesting();
      await microtasksFinished();

      assertEquals(
          hint,
          composebox.getInputElement().$.input.getAttribute('placeholder'));

      // Disable tool mode.
      composebox.onToolClick(new CustomEvent('tool-click', {
        bubbles: true,
        composed: true,
        detail: {toolMode: tool},
      }));
      await microtasksFinished();
      searchboxPageRemote.onInputStateChanged({
        ...new MockInputState(),
        activeTool: ToolMode.kUnspecified,
      });
      await searchboxPageRemote.$.flushForTesting();
      microtasksFinished();

      assertEquals(
          defaultApiHint,
          composebox.getInputElement().$.input.getAttribute('placeholder'));
    });
  });


  test(
      'updates suggestions when inputState activeTool changes from unspecified',
      async () => {
        // 1. Set an initial tool mode state on the element.
        const initialInputState: InputState = new MockInputState({
          activeTool: ToolMode.kUnspecified,
        });
        await setupComposeboxWithInputState(initialInputState);

        // 2. Override queryAutocomplete to track if it's called.
        let queryAutocompleteCalledWithTrue = false;
        composebox.queryAutocomplete = (clearMatches: boolean) => {
          queryAutocompleteCalledWithTrue = clearMatches;
        };

        // 3. Simulate backend updating the activeTool.
        const newInputState: InputState = new MockInputState({
          activeTool: ToolMode.kDeepSearch,  // Different tool
        });

        // Trigger the onInputStateChanged listener.
        searchboxPageRemote.onInputStateChanged(newInputState);

        // 4. Wait for any potential flushes and element updates.
        await searchboxPageRemote.$.flushForTesting();
        await microtasksFinished();
        await composebox.updateComplete;

        // 5. Assertions.
        assertEquals(ToolMode.kDeepSearch, composebox.inputState!.activeTool);
        assertTrue(queryAutocompleteCalledWithTrue);
      });

  test(
      'updates suggestions when inputState activeTool changes between tools',
      async () => {
        // 1. Set an initial tool mode state on the element.
        const initialInputState: InputState = new MockInputState({
          activeTool: ToolMode.kDeepSearch,
        });
        await setupComposeboxWithInputState(initialInputState);

        // 2. Override queryAutocomplete to track if it's called.
        let queryAutocompleteCalledWithTrue = false;
        composebox.queryAutocomplete = (clearMatches: boolean) => {
          queryAutocompleteCalledWithTrue = clearMatches;
        };

        // 3. Simulate backend updating the activeTool to another tool.
        const newInputState: InputState = new MockInputState({
          activeTool: ToolMode.kImageGen,
        });

        // Trigger the onInputStateChanged listener.
        searchboxPageRemote.onInputStateChanged(newInputState);

        // 4. Wait for any potential flushes and element updates.
        await searchboxPageRemote.$.flushForTesting();
        await microtasksFinished();
        await composebox.updateComplete;

        // 5. Assertions.
        assertEquals(ToolMode.kImageGen, composebox.inputState!.activeTool);
        assertTrue(queryAutocompleteCalledWithTrue);
      });
});

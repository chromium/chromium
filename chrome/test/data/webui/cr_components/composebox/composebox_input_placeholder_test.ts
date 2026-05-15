// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/composebox/composebox.js';

import type {ComposeboxFile} from 'chrome://resources/cr_components/composebox/common.js';
import type {ComposeboxElement} from 'chrome://resources/cr_components/composebox/composebox.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {ContextUploadStatus, ModelMode, ToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {InputState} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock, MockInputState} from './composebox_test_utils.js';

suite('ComposeboxInputPlaceholder', () => {
  let composebox: ComposeboxElement;
  let searchboxHandler: TestMock<SearchboxPageHandlerRemote>;
  let windowProxy: TestMock<WindowProxy>;
  let searchboxCallbackRouter: SearchboxPageCallbackRouter;
  let searchboxPageRemote: SearchboxPageRemote;

  async function setupComposeboxWithInputState(inputState: InputState) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    searchboxHandler.setResultFor(
        'getInputState', Promise.resolve({state: inputState}));

    composebox = document.createElement('cr-composebox');
    composebox.ntpRealboxNextEnabled = true;
    composebox.state = {
      text: '',
      files: [],
      mode: ToolMode.kUnspecified,
      model: ModelMode.kUnspecified,
    };

    document.body.appendChild(composebox);

    searchboxPageRemote.onInputStateChanged(inputState);

    await microtasksFinished();
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    searchboxCallbackRouter = new SearchboxPageCallbackRouter();
    searchboxPageRemote = searchboxCallbackRouter.$.bindNewPipeAndPassRemote();
    const composeboxHandler = installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new PageCallbackRouter(), new SearchboxPageHandlerRemote(),
            searchboxCallbackRouter)));
    composeboxHandler.setResultMapperFor(
        'getSmartTabSharingActive', () => Promise.resolve({active: false}));

    searchboxHandler = installMock(
        SearchboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.getInstance().searchboxHandler = mock);

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


    composebox = document.createElement('cr-composebox');
    document.body.appendChild(composebox);
    await microtasksFinished();
  });

  test('InputPlaceholderOverride', async () => {
    const input = composebox.getInputElement().$.input;
    assertTrue(!!input);
    const initialPlaceholder = input.placeholder;
    assertTrue(initialPlaceholder.length > 0);
    assertEquals('', composebox.inputPlaceholderOverride);

    const overrideText = 'Override Text';
    composebox.inputPlaceholderOverride = overrideText;
    await composebox.updateComplete;

    assertEquals(overrideText, input.placeholder);

    composebox.inputPlaceholderOverride = '';
    await composebox.updateComplete;

    assertEquals(initialPlaceholder, input.placeholder);
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
      }],
    });

    await setupComposeboxWithInputState(testInputState);
    assertEquals(modelHint, composebox.getInputElement().$.input.placeholder);
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
                                    })),
      }));

      // Initial placeholder check.
      assertEquals(
          defaultApiHint, composebox.getInputElement().$.input.placeholder);

      // Enable tool mode.
      const contextEntrypoint =
          composebox.shadowRoot.querySelector('#contextEntrypoint');
      assertTrue(!!contextEntrypoint);
      contextEntrypoint.dispatchEvent(new CustomEvent('tool-click', {
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

      assertEquals(hint, composebox.getInputElement().$.input.placeholder);

      // Disable tool mode.
      contextEntrypoint.dispatchEvent(new CustomEvent('tool-click', {
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
          defaultApiHint, composebox.getInputElement().$.input.placeholder);
    });
  });

  test('MultipleFilesUpdatesPlaceholder', async () => {
    loadTimeData.overrideValues({
      composeboxHintTextAskAboutThese: 'Ask about these',
    });
    composebox.enableFileHint = true;
    const token1 = {high: 0n, low: 1n} as any;
    const token2 = {high: 0n, low: 2n} as any;
    composebox.addFileContextForTesting({
      type: 'image/png',
      uuid: token1,
      status: ContextUploadStatus.kNotUploaded,
    } as ComposeboxFile);
    composebox.addFileContextForTesting({
      type: 'application/pdf',
      uuid: token2,
      status: ContextUploadStatus.kNotUploaded,
    } as ComposeboxFile);
    await composebox.updateComplete;

    assertEquals(
        'Ask about these', composebox.getInputElement().$.input.placeholder);
  });

  test('SingleTabFileUpdatesPlaceholder', async () => {
    loadTimeData.overrideValues({
      composeboxHintTextAskAboutThisTab: 'Ask about this tab',
    });
    composebox.enableFileHint = true;
    const token = {high: 0n, low: 1n} as any;
    composebox.addFileContextForTesting({
      type: 'tab',
      uuid: token,
      status: ContextUploadStatus.kNotUploaded,
    } as ComposeboxFile);
    await composebox.updateComplete;

    assertEquals(
        'Ask about this tab', composebox.getInputElement().$.input.placeholder);
  });

  test('SingleAutoTabFileDoesNotUpdatePlaceholder', async () => {
    loadTimeData.overrideValues({
      composeboxHintTextAskAboutThisTab: 'Ask about this tab',
    });
    composebox.enableFileHint = true;
    const token = {high: 0n, low: 1n};
    searchboxHandler.setResultFor('addTabContext', Promise.resolve({token}));

    const tab: TabInfo = {
      tabId: 1,
      title: 'Auto Tab',
      url: 'http://example.com',
      showInCurrentTabChip: false,
      showInPreviousTabChip: false,
      lastActive: {internalValue: BigInt(1)} as any,
    };

    composebox.updateAutoSuggestedTabContextForTesting(tab);
    await searchboxHandler.whenCalled('addTabContext');
    await composebox.updateComplete;

    assertEquals(
        defaultApiHint, composebox.getInputElement().$.input.placeholder);
  });

  test('SingleImageFileUpdatesPlaceholder', async () => {
    loadTimeData.overrideValues({
      composeboxHintTextAskAboutThisImage: 'Ask about this image',
    });
    composebox.enableFileHint = true;
    const token = {high: 0n, low: 1n} as any;
    composebox.addFileContextForTesting({
      type: 'image/png',
      uuid: token,
      status: ContextUploadStatus.kNotUploaded,
    } as ComposeboxFile);
    await composebox.updateComplete;

    assertEquals(
        'Ask about this image',
        composebox.getInputElement().$.input.placeholder);
  });

  test('SinglePdfFileUpdatesPlaceholder', async () => {
    loadTimeData.overrideValues({
      composeboxHintTextAskAboutThisDoc: 'Ask about this doc',
    });
    composebox.enableFileHint = true;
    const token = {high: 0n, low: 1n} as any;
    composebox.addFileContextForTesting({
      type: 'application/pdf',
      uuid: token,
      status: ContextUploadStatus.kNotUploaded,
    } as ComposeboxFile);
    await composebox.updateComplete;

    assertEquals(
        'Ask about this doc', composebox.getInputElement().$.input.placeholder);
  });

  test('SingleUnknownFileUpdatesPlaceholder', async () => {
    composebox.enableFileHint = true;
    const token = {high: 0n, low: 1n} as any;
    composebox.addFileContextForTesting({
      type: 'unknown/type',
      uuid: token,
      status: ContextUploadStatus.kNotUploaded,
    } as ComposeboxFile);
    await composebox.updateComplete;

    const placeholder = composebox.getInputElement().$.input.placeholder;
    assertTrue(
        !placeholder.includes('Ask about'),
        `Placeholder '${placeholder}' should not include 'Ask about'`);
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

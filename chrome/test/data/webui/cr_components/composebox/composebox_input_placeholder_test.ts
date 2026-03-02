// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/composebox/composebox.js';

import type {ComposeboxElement} from 'chrome://resources/cr_components/composebox/composebox.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {ModelMode, ToolMode as ComposeboxToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {InputState} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createInputState, installMock} from './composebox_test_utils.js';

suite('ComposeboxInputPlaceholder', () => {
  let composebox: ComposeboxElement;
  let searchboxHandler: TestMock<SearchboxPageHandlerRemote>;
  let windowProxy: TestMock<WindowProxy>;

  async function setupComposeboxWithInputState(inputState: InputState) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    composebox = document.createElement('cr-composebox');
    composebox.ntpRealboxNextEnabled = true;

    // We need to wait for the event to be fired when the element is connected.
    const whenInitialized = new Promise<CustomEvent>((resolve) => {
      composebox.addEventListener('composebox-initialized', (e: Event) => {
        resolve(e as CustomEvent);
      }, {once: true});
    });

    document.body.appendChild(composebox);
    const event = await whenInitialized;

    // Call the callback to initialize state.
    event.detail.initializeComposeboxState(
        '', [], ComposeboxToolMode.kUnspecified, ModelMode.kUnspecified,
        inputState);
    await microtasksFinished();
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new PageCallbackRouter(), new SearchboxPageHandlerRemote(),
            new SearchboxPageCallbackRouter())));

    searchboxHandler = installMock(
        SearchboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.getInstance().searchboxHandler = mock);

    searchboxHandler.setResultFor('getRecentTabs', Promise.resolve({tabs: []}));
    searchboxHandler.setResultFor(
        'getInputState', Promise.resolve({state: createInputState()}));

    windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('setTimeout', 0);

    composebox = document.createElement('cr-composebox');
    document.body.appendChild(composebox);
    await microtasksFinished();
  });

  test('InputPlaceholderOverride', async () => {
    const input = composebox.$.input;
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
    const testInputState = createInputState({
      activeModel: ModelMode.kGeminiRegular,
      modelConfigs: [{
        model: ModelMode.kGeminiRegular,
        hintText: modelHint,
        menuLabel: '',
        aimUrlParams: [],
      }],
    });

    await setupComposeboxWithInputState(testInputState);
    assertEquals(modelHint, composebox.$.input.placeholder);
  });

  const defaultApiHint = loadTimeData.getString('searchboxComposePlaceholder');

  const toolConfigTestCases = [
    {
      tool: ComposeboxToolMode.kDeepSearch,
      hint: 'Research anything',
      name: 'DeepSearch',
    },
    {
      tool: ComposeboxToolMode.kImageGen,
      hint: 'Describe your image',
      name: 'ImageGen',
    },
    {
      tool: ComposeboxToolMode.kCanvas,
      hint: 'Create anything',
      name: 'Canvas',
    },
  ];

  toolConfigTestCases.forEach(({tool, hint, name}) => {
    test(`InputPlaceholderFromToolConfig_${name}`, async () => {
      const mockInputState = createInputState({
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
      });

      await setupComposeboxWithInputState(mockInputState);

      // Initial placeholder check.
      assertEquals(defaultApiHint, composebox.$.input.placeholder);

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
      assertEquals(hint, composebox.$.input.placeholder);

      // Disable tool mode.
      contextEntrypoint.dispatchEvent(new CustomEvent('tool-click', {
        bubbles: true,
        composed: true,
        detail: {toolMode: tool},
      }));
      await microtasksFinished();
      assertEquals(defaultApiHint, composebox.$.input.placeholder);
    });
  });
});

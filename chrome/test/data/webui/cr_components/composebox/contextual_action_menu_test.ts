// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/composebox/contextual_action_menu.js';

import type {ContextualActionMenuElement} from 'chrome://resources/cr_components/composebox/contextual_action_menu.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {InputType, ModelMode, ToolMode} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createInputState} from './composebox_test_utils.js';

suite('ContextualActionMenu', () => {
  let actionMenu: ContextualActionMenuElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      composeboxContextMenuEnableMultiTabSelection: true,
      composeboxFileMaxCount: 10,
      composeboxShowContextMenuTabPreviews: true,
      composeboxShowPdfUpload: true,
      ShowContextMenuHeaders: true,
    });

    actionMenu = document.createElement('cr-composebox-contextual-action-menu');
    Object.assign(
        actionMenu,
        {fileNum: 0, disabledTabIds: new Map(), tabSuggestions: []});
    document.body.appendChild(actionMenu);
    await microtasksFinished();
  });

  test('menu is hidden initially', async () => {
    await microtasksFinished();
    assertFalse(actionMenu.$.menu.open);
  });

  test(
      'No tabs or tab header displayed when there are no tab suggestions',
      async () => {
        // Arrange & Act.
        actionMenu.tabSuggestions = [];
        actionMenu.showAt(actionMenu);
        await microtasksFinished();
        assertTrue(actionMenu.$.menu.open);

        // Assert.
        const tabHeader = $$(actionMenu, '#tabHeader');
        assertFalse(!!tabHeader);
        const items = actionMenu.$.menu.querySelectorAll('.dropdown-item');
        assertEquals(0, items.length);
      });

  test('Shows all allowed tools and models', async () => {
    actionMenu.inputState = createInputState({
      allowedTools: [ToolMode.kDeepSearch, ToolMode.kImageGen],
      toolConfigs: [
        {
          tool: ToolMode.kDeepSearch,
          menuLabel: 'Deep Search',
          disableActiveModelSelection: false,
          chipLabel: '',
          hintText: '',
          aimUrlParams: [],
        },
        {
          tool: ToolMode.kImageGen,
          menuLabel: 'Generate Image',
          disableActiveModelSelection: false,
          chipLabel: '',
          hintText: '',
          aimUrlParams: [],
        },
      ],
      allowedModels: [ModelMode.kGeminiRegular, ModelMode.kGeminiPro],
      modelConfigs: [
        {
          model: ModelMode.kGeminiRegular,
          menuLabel: 'Gemini Regular',
          hintText: '',
          aimUrlParams: [],
        },
        {
          model: ModelMode.kGeminiPro,
          menuLabel: 'Gemini Pro',
          hintText: '',
          aimUrlParams: [],
        },
      ],
    });
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    assertTrue(!!$$(actionMenu, `[data-mode="${ToolMode.kDeepSearch}"]`));
    assertTrue(!!$$(actionMenu, `[data-mode="${ToolMode.kImageGen}"]`));
    assertTrue(!!$$(actionMenu, `[data-model="${ModelMode.kGeminiRegular}"]`));
    assertTrue(!!$$(actionMenu, `[data-model="${ModelMode.kGeminiPro}"]`));

    assertEquals(
        'menuitem',
        $$(actionMenu, `[data-mode="${ToolMode.kDeepSearch}"]`)!
            .getAttribute('role'));
    assertEquals(
        'menuitemradio',
        $$(actionMenu, `[data-model="${ModelMode.kGeminiRegular}"]`)!
            .getAttribute('role'));
    assertEquals(
        'menuitemradio',
        $$(actionMenu, `[data-model="${ModelMode.kGeminiPro}"]`)!
            .getAttribute('role'));
  });

  test('Hides tools and models not allowed', async () => {
    actionMenu.inputState = createInputState({
      allowedTools: [ToolMode.kDeepSearch],
      toolConfigs: [
        {
          tool: ToolMode.kDeepSearch,
          menuLabel: 'Deep Search',
          disableActiveModelSelection: false,
          chipLabel: '',
          hintText: '',
          aimUrlParams: [],
        },
        {
          tool: ToolMode.kImageGen,
          menuLabel: 'Generate Image',
          disableActiveModelSelection: false,
          chipLabel: '',
          hintText: '',
          aimUrlParams: [],
        },
      ],
      allowedModels: [ModelMode.kGeminiRegular],
      modelConfigs: [
        {
          model: ModelMode.kGeminiRegular,
          menuLabel: 'Gemini Regular',
          hintText: '',
          aimUrlParams: [],
        },
        {
          model: ModelMode.kGeminiPro,
          menuLabel: 'Gemini Pro',
          hintText: '',
          aimUrlParams: [],
        },
      ],
    });
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    assertTrue(!!$$(actionMenu, `[data-mode="${ToolMode.kDeepSearch}"]`));
    assertFalse(!!$$(actionMenu, `[data-mode="${ToolMode.kImageGen}"]`));
    assertTrue(!!$$(actionMenu, `[data-model="${ModelMode.kGeminiRegular}"]`));
    assertFalse(!!$$(actionMenu, `[data-model="${ModelMode.kGeminiPro}"]`));
  });

  test('Disables disabled tools and models', async () => {
    actionMenu.inputState = createInputState({
      allowedTools: [ToolMode.kDeepSearch, ToolMode.kImageGen],
      disabledTools: [ToolMode.kImageGen],
      toolConfigs: [
        {
          tool: ToolMode.kDeepSearch,
          menuLabel: 'Deep Search',
          disableActiveModelSelection: false,
          chipLabel: '',
          hintText: '',
          aimUrlParams: [],
        },
        {
          tool: ToolMode.kImageGen,
          menuLabel: 'Generate Image',
          disableActiveModelSelection: false,
          chipLabel: '',
          hintText: '',
          aimUrlParams: [],
        },
      ],
      allowedModels: [ModelMode.kGeminiRegular, ModelMode.kGeminiPro],
      disabledModels: [ModelMode.kGeminiPro],
      modelConfigs: [
        {
          model: ModelMode.kGeminiRegular,
          menuLabel: 'Gemini Regular',
          hintText: '',
          aimUrlParams: [],
        },
        {
          model: ModelMode.kGeminiPro,
          menuLabel: 'Gemini Pro',
          hintText: '',
          aimUrlParams: [],
        },
      ],
    });
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    const deepSearch =
        $$(actionMenu, `[data-mode="${ToolMode.kDeepSearch}"]`) as
        HTMLButtonElement;
    const createImages =
        $$(actionMenu, `[data-mode="${ToolMode.kImageGen}"]`) as
        HTMLButtonElement;
    const regularModel =
        $$(actionMenu, `[data-model="${ModelMode.kGeminiRegular}"]`) as
        HTMLButtonElement;
    const thinkingModel =
        $$(actionMenu, `[data-model="${ModelMode.kGeminiPro}"]`) as
        HTMLButtonElement;

    assertFalse(deepSearch.disabled);
    assertTrue(createImages.disabled);
    assertFalse(regularModel.disabled);
    assertTrue(thinkingModel.disabled);
  });

  test('Shows active model checkmark', async () => {
    actionMenu.inputState = createInputState({
      allowedModels: [ModelMode.kGeminiRegular, ModelMode.kGeminiPro],
      activeModel: ModelMode.kGeminiPro,
      modelConfigs: [
        {
          model: ModelMode.kGeminiRegular,
          menuLabel: 'Gemini Regular',
          hintText: '',
          aimUrlParams: [],
        },
        {
          model: ModelMode.kGeminiPro,
          menuLabel: 'Gemini Pro',
          hintText: '',
          aimUrlParams: [],
        },
      ],
    });
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    const regularModel =
        $$(actionMenu, `[data-model="${ModelMode.kGeminiRegular}"]`)!;
    const thinkingModel =
        $$(actionMenu, `[data-model="${ModelMode.kGeminiPro}"]`)!;

    assertFalse(!!regularModel.querySelector('#model-check'));
    assertTrue(!!thinkingModel.querySelector('#model-check'));

    assertEquals('false', regularModel.getAttribute('aria-checked'));
    assertEquals('true', thinkingModel.getAttribute('aria-checked'));
  });

  test('Shows image and file upload when allowed', async () => {
    actionMenu.inputState = createInputState({
      allowedInputTypes: [InputType.kLensImage, InputType.kLensFile],
    });
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    assertTrue(!!$$(actionMenu, '#imageUpload'));
    assertTrue(!!$$(actionMenu, '#fileUpload'));

    assertEquals(
        'menuitem', $$(actionMenu, '#imageUpload')!.getAttribute('role'));
    assertEquals(
        'menuitem', $$(actionMenu, '#fileUpload')!.getAttribute('role'));
  });

  test('Hides image and file upload when not allowed', async () => {
    actionMenu.inputState = createInputState({
      allowedInputTypes: [],
    });
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    assertFalse(!!$$(actionMenu, '#imageUpload'));
    assertFalse(!!$$(actionMenu, '#fileUpload'));
  });

  test('Disables image and file upload when disabled', async () => {
    actionMenu.inputState = createInputState({
      allowedInputTypes: [InputType.kLensImage, InputType.kLensFile],
      disabledInputTypes: [InputType.kLensImage, InputType.kLensFile],
    });
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    const imageUpload = $$(actionMenu, '#imageUpload') as HTMLButtonElement;
    const fileUpload = $$(actionMenu, '#fileUpload') as HTMLButtonElement;

    assertTrue(imageUpload.disabled);
    assertTrue(fileUpload.disabled);
  });

  test('Shows models only when tools are disallowed', async () => {
    actionMenu.inputState = createInputState({
      allowedTools: [],
      toolConfigs: [],
      allowedModels: [ModelMode.kGeminiRegular],
      modelConfigs: [{
        model: ModelMode.kGeminiRegular,
        menuLabel: 'Gemini Regular',
        hintText: '',
        aimUrlParams: [],
      }],
      modelSectionConfig: {header: 'Models'},
    });
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    assertFalse(!!$$(actionMenu, `[data-mode="${ToolMode.kDeepSearch}"]`));
    assertTrue(!!$$(actionMenu, `[data-model="${ModelMode.kGeminiRegular}"]`));
    assertTrue(!!$$(actionMenu, '#modelHeader'));
  });

  test('Shows tools only when models are disallowed', async () => {
    actionMenu.inputState = createInputState({
      allowedTools: [ToolMode.kDeepSearch],
      toolConfigs: [{
        tool: ToolMode.kDeepSearch,
        menuLabel: 'Deep Search',
        disableActiveModelSelection: false,
        chipLabel: '',
        hintText: '',
        aimUrlParams: [],
      }],
      allowedModels: [],
      modelConfigs: [],
    });
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    assertTrue(!!$$(actionMenu, `[data-mode="${ToolMode.kDeepSearch}"]`));
    assertFalse(
        !!$$(actionMenu, `[data-model="${ModelMode.kGeminiRegular}"]`));
    assertFalse(!!$$(actionMenu, '#modelHeader'));
  });

  test('Handles both tools and models disallowed', async () => {
    actionMenu.inputState = createInputState({
      allowedTools: [],
      toolConfigs: [],
      allowedModels: [],
      modelConfigs: [],
    });
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    // Verify no tools are shown.
    assertFalse(!!$$(actionMenu, `[data-mode="${ToolMode.kDeepSearch}"]`));
    assertFalse(!!$$(actionMenu, `[data-mode="${ToolMode.kImageGen}"]`));
    assertFalse(!!$$(actionMenu, `[data-mode="${ToolMode.kCanvas}"]`));

    // Verify no models are shown.
    assertFalse(
        !!$$(actionMenu, `[data-model="${ModelMode.kGeminiRegular}"]`));
    assertFalse(
        !!$$(actionMenu, `[data-model="${ModelMode.kGeminiProAutoroute}"]`));
    assertFalse(!!$$(actionMenu, `[data-model="${ModelMode.kGeminiPro}"]`));
    assertFalse(!!$$(actionMenu, '#modelHeader'));

    const menu = actionMenu.$.menu;
    // No separator should be shown if there are no tools/models and no uploads
    const hr = menu.querySelector('hr');
    assertFalse(!!hr);
  });

  test('Handles single tool allowed', async () => {
    actionMenu.inputState = createInputState({
      allowedTools: [ToolMode.kCanvas],
      toolConfigs: [{
        tool: ToolMode.kCanvas,
        menuLabel: 'Canvas',
        disableActiveModelSelection: false,
        chipLabel: '',
        hintText: '',
        aimUrlParams: [],
      }],
    });
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    assertTrue(!!$$(actionMenu, `[data-mode="${ToolMode.kCanvas}"]`));
    assertFalse(!!$$(actionMenu, `[data-mode="${ToolMode.kDeepSearch}"]`));
  });

  test('Handles single model allowed', async () => {
    actionMenu.inputState = createInputState({
      allowedModels: [ModelMode.kGeminiProAutoroute],
      modelConfigs: [{
        model: ModelMode.kGeminiProAutoroute,
        menuLabel: 'Gemini Auto',
        hintText: '',
        aimUrlParams: [],
      }],
    });
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    assertTrue(
        !!$$(actionMenu, `[data-model="${ModelMode.kGeminiProAutoroute}"]`));
    assertFalse(
        !!$$(actionMenu, `[data-model="${ModelMode.kGeminiRegular}"]`));
  });

  test('Browser tab suggestions respect allowedInputTypes', async () => {
    // Arrange: Provide tab suggestions but disallow browser tabs in InputState.
    const tabInfo = {
      tabId: 1,
      title: 'Google',
      url: 'https://google.com',
      lastActiveTime: {internalValue: 0n},
      showInCurrentTabChip: false,
      showInPreviousTabChip: false,
      lastActive: {internalValue: 0n},
    };
    actionMenu.tabSuggestions = [tabInfo];
    actionMenu.inputState = createInputState({
      allowedInputTypes: [],  // kBrowserTab is missing
    });

    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    // Assert: Tab suggestions should not be shown.
    const items = actionMenu.$.menu.querySelectorAll('.dropdown-item');
    assertEquals(0, items.length);
  });

  test('Browser tab suggestions shown when allowed', async () => {
    // Arrange: Provide tab suggestions and allow browser tabs.
    const tabInfo = {
      tabId: 1,
      title: 'Google',
      url: 'https://google.com',
      lastActiveTime: {internalValue: 0n},
      showInCurrentTabChip: false,
      showInPreviousTabChip: false,
      lastActive: {internalValue: 0n},
    };
    actionMenu.tabSuggestions = [tabInfo];
    actionMenu.inputState = createInputState({
      allowedInputTypes: [InputType.kBrowserTab],
    });

    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    // Assert: Tab suggestions should be shown.
    const items = actionMenu.$.menu.querySelectorAll('.dropdown-item');
    // 1 tab item.
    assertEquals(1, items.length);

    const tabButton = items[0] as HTMLButtonElement;
    assertEquals('menuitemcheckbox', tabButton.getAttribute('role'));
    assertEquals('false', tabButton.getAttribute('aria-checked'));

    // Check with selection.
    actionMenu.disabledTabIds = new Map([[tabInfo.tabId, '1']]);
    await microtasksFinished();
    assertEquals('true', tabButton.getAttribute('aria-checked'));
  });

  test('Uses configured menu labels', async () => {
    const deepSearchLabel = 'Custom Deep Search Label';
    const geminiLabel = 'Custom Gemini Label';
    const imageUploadLabel = 'Custom Image Upload Label';

    actionMenu.inputState = createInputState({
      allowedTools: [ToolMode.kDeepSearch],
      toolConfigs: [{
        tool: ToolMode.kDeepSearch,
        menuLabel: deepSearchLabel,
        disableActiveModelSelection: false,
        chipLabel: '',
        hintText: '',
        aimUrlParams: [],
      }],
      allowedModels: [ModelMode.kGeminiRegular],
      modelConfigs: [{
        model: ModelMode.kGeminiRegular,
        menuLabel: geminiLabel,
        hintText: '',
        aimUrlParams: [],
      }],
      allowedInputTypes: [InputType.kLensImage],
      inputTypeConfigs: [{
        inputType: InputType.kLensImage,
        menuLabel: imageUploadLabel,
      }],
    });
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    const deepSearch = $$(actionMenu, `[data-mode="${ToolMode.kDeepSearch}"]`);
    const geminiRegular =
        $$(actionMenu, `[data-model="${ModelMode.kGeminiRegular}"]`);
    const imageUpload = $$(actionMenu, '#imageUpload');

    assertTrue(deepSearch!.textContent.includes(deepSearchLabel));
    assertTrue(geminiRegular!.textContent.includes(geminiLabel));
    assertTrue(imageUpload!.textContent.includes(imageUploadLabel));
  });
});

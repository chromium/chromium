// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/composebox/contextual_action_menu.js';

import type {ContextualActionMenuElement} from 'chrome://resources/cr_components/composebox/contextual_action_menu.js';
import type {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {InputType, ModelMode, ToolMode} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {MockInputState} from './composebox_test_utils.js';

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
      composeboxSmartTabSharingVisible: false,
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
    actionMenu.inputState = new MockInputState({
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
      toolsSectionConfig: {header: ''},
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
      modelSectionConfig: {header: ''},
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
    actionMenu.inputState = new MockInputState({
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
      toolsSectionConfig: {header: ''},
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
      modelSectionConfig: {header: ''},
    });
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    assertTrue(!!$$(actionMenu, `[data-mode="${ToolMode.kDeepSearch}"]`));
    assertFalse(!!$$(actionMenu, `[data-mode="${ToolMode.kImageGen}"]`));
    assertTrue(!!$$(actionMenu, `[data-model="${ModelMode.kGeminiRegular}"]`));
    assertFalse(!!$$(actionMenu, `[data-model="${ModelMode.kGeminiPro}"]`));
  });

  test('Disables disabled tools and models', async () => {
    actionMenu.inputState = new MockInputState({
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
      toolsSectionConfig: {header: ''},
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
      modelSectionConfig: {header: ''},
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
    actionMenu.inputState = new MockInputState({
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
      modelSectionConfig: {header: ''},
      toolsSectionConfig: {header: ''},
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
    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [InputType.kLensImage, InputType.kLensFile],
      toolsSectionConfig: {header: ''},
      modelSectionConfig: {header: ''},
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

  test('Shows drive when allowed', async () => {
    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [InputType.kDrive],
      toolsSectionConfig: {header: ''},
      modelSectionConfig: {header: ''},
    });
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    assertTrue(!!$$(actionMenu, '#driveUpload'));
    assertEquals(
        'menuitem', $$(actionMenu, '#driveUpload')!.getAttribute('role'));
  });

  test('Hides image and file upload when not allowed', async () => {
    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [],
      toolsSectionConfig: {header: ''},
      modelSectionConfig: {header: ''},
    });
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    assertFalse(!!$$(actionMenu, '#imageUpload'));
    assertFalse(!!$$(actionMenu, '#fileUpload'));
    assertFalse(!!$$(actionMenu, '#driveUpload'));
  });

  test('Disables image and file upload when disabled', async () => {
    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [InputType.kLensImage, InputType.kLensFile],
      disabledInputTypes: [InputType.kLensImage, InputType.kLensFile],
      toolsSectionConfig: {header: ''},
      modelSectionConfig: {header: ''},
    });
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    const imageUpload = $$(actionMenu, '#imageUpload') as HTMLButtonElement;
    const fileUpload = $$(actionMenu, '#fileUpload') as HTMLButtonElement;

    assertTrue(imageUpload.disabled);
    assertTrue(fileUpload.disabled);
  });

  test('Shows models only when tools are disallowed', async () => {
    actionMenu.inputState = new MockInputState({
      allowedTools: [],
      toolConfigs: [],
      toolsSectionConfig: {header: ''},
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
    actionMenu.inputState = new MockInputState({
      allowedTools: [ToolMode.kDeepSearch],
      toolConfigs: [{
        tool: ToolMode.kDeepSearch,
        menuLabel: 'Deep Search',
        disableActiveModelSelection: false,
        chipLabel: '',
        hintText: '',
        aimUrlParams: [],
      }],
      toolsSectionConfig: {header: ''},
      allowedModels: [],
      modelConfigs: [],
      modelSectionConfig: {header: ''},
    });
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    assertTrue(!!$$(actionMenu, `[data-mode="${ToolMode.kDeepSearch}"]`));
    assertFalse(
        !!$$(actionMenu, `[data-model="${ModelMode.kGeminiRegular}"]`));
    assertFalse(!!$$(actionMenu, '#modelHeader'));
  });

  test('Handles both tools and models disallowed', async () => {
    actionMenu.inputState = new MockInputState({
      allowedTools: [],
      toolConfigs: [],
      toolsSectionConfig: {header: ''},
      allowedModels: [],
      modelConfigs: [],
      modelSectionConfig: {header: ''},
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
    actionMenu.inputState = new MockInputState({
      allowedTools: [ToolMode.kCanvas],
      toolConfigs: [{
        tool: ToolMode.kCanvas,
        menuLabel: 'Canvas',
        disableActiveModelSelection: false,
        chipLabel: '',
        hintText: '',
        aimUrlParams: [],
      }],
      toolsSectionConfig: {header: ''},
      modelSectionConfig: {header: ''},
    });
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    assertTrue(!!$$(actionMenu, `[data-mode="${ToolMode.kCanvas}"]`));
    assertFalse(!!$$(actionMenu, `[data-mode="${ToolMode.kDeepSearch}"]`));
  });

  test('Handles single model allowed', async () => {
    actionMenu.inputState = new MockInputState({
      allowedModels: [ModelMode.kGeminiProAutoroute],
      modelConfigs: [{
        model: ModelMode.kGeminiProAutoroute,
        menuLabel: 'Gemini Auto',
        hintText: '',
        aimUrlParams: [],
      }],
      modelSectionConfig: {header: ''},
      toolsSectionConfig: {header: ''},
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
    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [],  // kBrowserTab is missing
      toolsSectionConfig: {header: ''},
      modelSectionConfig: {header: ''},
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
    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [InputType.kBrowserTab],
      toolsSectionConfig: {header: ''},
      modelSectionConfig: {header: ''},
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
    const toolsHeader = 'Tools Header';
    const deepSearchLabel = 'Custom Deep Search Label';
    const geminiLabel = 'Custom Gemini Label';
    const imageUploadLabel = 'Custom Image Upload Label';

    actionMenu.inputState = new MockInputState({
      allowedTools: [ToolMode.kDeepSearch],
      toolConfigs: [{
        tool: ToolMode.kDeepSearch,
        menuLabel: deepSearchLabel,
        disableActiveModelSelection: false,
        chipLabel: '',
        hintText: '',
        aimUrlParams: [],
      }],
      toolsSectionConfig: {header: toolsHeader},
      allowedModels: [ModelMode.kGeminiRegular],
      modelConfigs: [{
        model: ModelMode.kGeminiRegular,
        menuLabel: geminiLabel,
        hintText: '',
        aimUrlParams: [],
      }],
      modelSectionConfig: {header: ''},
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
    assertEquals(
        `${toolsHeader}: ${deepSearchLabel}`,
        deepSearch!.getAttribute('aria-label'));
    assertTrue(geminiRegular!.textContent.includes(geminiLabel));
    assertEquals(
        `${geminiLabel}`,
        geminiRegular!.getAttribute('aria-label'));
    assertTrue(imageUpload!.textContent.includes(imageUploadLabel));
  });

  test('Toggling smart tab sharing fires event', async () => {
    loadTimeData.overrideValues({composeboxSmartTabSharingVisible: true});
    actionMenu.remove();
    actionMenu = document.createElement('cr-composebox-contextual-action-menu');
    document.body.appendChild(actionMenu);

    actionMenu.smartTabSharingActive = false;
    actionMenu.showAt(actionMenu);
    await actionMenu.updateComplete;

    const toggle = $$(actionMenu, '#smartTabSharingToggle') as CrToggleElement;
    assertTrue(!!toggle);

    let eventDetail: {active: boolean}|null = null;
    actionMenu.addEventListener(
        'smart-tab-sharing-active-changed', (e: Event) => {
          eventDetail = (e as CustomEvent).detail;
        }, {once: true});

    toggle.checked = true;
    toggle.dispatchEvent(new CustomEvent('change'));

    assertTrue(!!eventDetail);
    assertTrue((eventDetail as {active: boolean}).active);
  });

  test('Clicking smart tab sharing row toggles state', async () => {
    loadTimeData.overrideValues({composeboxSmartTabSharingVisible: true});
    actionMenu.remove();
    actionMenu = document.createElement('cr-composebox-contextual-action-menu');
    document.body.appendChild(actionMenu);

    actionMenu.smartTabSharingActive = false;
    actionMenu.showAt(actionMenu);
    await actionMenu.updateComplete;

    const item = $$(actionMenu, '#smartTabSharingItem') as HTMLElement;
    assertTrue(!!item);
    const toggle = $$(actionMenu, '#smartTabSharingToggle') as CrToggleElement;

    let eventFired = false;
    actionMenu.addEventListener('smart-tab-sharing-active-changed', () => {
      eventFired = true;
    }, {once: true});

    item.click();

    assertTrue(eventFired);
    assertTrue(toggle.checked);
  });

  test('AutoRepositionEnabledByDefaultOnSharedWrapper', () => {
    const innerMenu = actionMenu.$.menu;
    assertTrue(innerMenu.hasAttribute('auto-reposition'));
    assertTrue(innerMenu.autoReposition);
  });

  test('AutoRepositionDisabledWhenOptedOut', async () => {
    actionMenu.remove();
    actionMenu = document.createElement('cr-composebox-contextual-action-menu');
    actionMenu.disableAutoReposition = true;
    document.body.appendChild(actionMenu);
    await microtasksFinished();

    const innerMenu = actionMenu.$.menu;
    assertFalse(innerMenu.hasAttribute('auto-reposition'));
    assertFalse(innerMenu.autoReposition);
  });

  test('Share tabs flyout height fits content', async () => {
    loadTimeData.overrideValues({
      contextManagementInComposeboxEnabled: true,
    });
    actionMenu.remove();
    actionMenu = document.createElement('cr-composebox-contextual-action-menu');
    actionMenu.tabSuggestions = [
      {
        tabId: 1,
        title: 'Tab 1',
        url: {url: 'https://example.com'},
        lastActiveTime: {internalValue: 0n},
        showInCurrentTabChip: false,
        showInPreviousTabChip: false,
        lastActive: {internalValue: 0n},
      } as any,
    ];
    actionMenu.inputState = new MockInputState({
                              allowedInputTypes: [InputType.kBrowserTab],
                            }) as any;
    document.body.appendChild(actionMenu);
    await microtasksFinished();

    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    const trigger = $$(actionMenu, '#shareTabsTrigger') as HTMLElement;
    assertTrue(!!trigger);
    trigger.dispatchEvent(new PointerEvent('pointerenter'));
    await microtasksFinished();

    const flyout = $$(actionMenu, '.share-tabs-flyout') as HTMLElement;
    assertTrue(!!flyout);
    assertFalse(flyout.hidden);

    actionMenu.tabSuggestions = Array(10).fill({
      tabId: 1,
      title: 'Tab',
      url: {url: 'https://example.com'},
      lastActiveTime: {internalValue: 0n},
      showInCurrentTabChip: false,
      showInPreviousTabChip: false,
      lastActive: {internalValue: 0n},
    });
    await microtasksFinished();

    // Ensure flyout has max height even with many tab suggestions.
    assertEquals(144, flyout.offsetHeight);
  });
});

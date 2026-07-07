// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';
import 'chrome://resources/cr_components/composebox/contextual_action_menu.js';
import 'chrome://resources/cr_components/composebox/composebox_favicon_group.js';

import type {ComposeboxFaviconGroupElement} from 'chrome://resources/cr_components/composebox/composebox_favicon_group.js';
import type {ContextualActionMenuElement} from 'chrome://resources/cr_components/composebox/contextual_action_menu.js';
import {DEFAULT_FLYOUT_WIDTH_PX, MIN_MENU_HEIGHT_PX, SHARE_TABS_FLYOUT_GAP_PX, SHARE_TABS_FLYOUT_MAX_HEIGHT_PX, VIEWPORT_BUFFER_PX, DEFAULT_MAX_MENU_HEIGHT_PX} from 'chrome://resources/cr_components/composebox/contextual_action_menu.js';
import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {InputState} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {InputType, ModelMode, ToolMode} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {$$, eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {MockInputState} from './composebox_test_utils.js';

function createTabSuggestion(overrides: Partial<TabInfo> = {}): TabInfo {
  return Object.assign(
      {
        tabId: 0,
        title: '',
        url: 'about:blank',
        showInCurrentTabChip: false,
        showInPreviousTabChip: false,
        lastActive: {internalValue: 0n},
      },
      overrides);
}

function triggerKeyDown(
    element: HTMLElement, key: string, shiftKey: boolean = false) {
  element.dispatchEvent(new KeyboardEvent('keydown', {
    key,
    code: key,
    shiftKey,
    bubbles: true,
    composed: true,
    cancelable: true,
  }));
}

interface InternalContextualActionMenu {
  onWindowBlur_: () => void;
  getSelectedTabs_: () => TabInfo[];
  resetShareTabsFlyout_: () => void;
  updateFlyoutPosition_: () => void;
  scheduleCloseTimer_: () => void;
  metricsSource_: string;
  closeMenuOnSelect: boolean;
  addTabContext_: (tabInfo: TabInfo) => void;
  deleteTabContext_: (uuid: string) => void;
}

function asInternal(element: ContextualActionMenuElement):
    InternalContextualActionMenu {
  return element as unknown as InternalContextualActionMenu;
}

suite('ContextualActionMenu', () => {
  let actionMenu: ContextualActionMenuElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      composeboxContextMenuEnableMultiTabSelection: true,
      composeboxFileMaxCount: 10,
      composeboxShowContextMenuTabPreviews: true,
      ShowContextMenuHeaders: true,
      contextManagementInComposeboxEnabled: false,
    });

    const pluralStringProxy = new TestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralStringProxy);

    actionMenu = document.createElement('cr-composebox-contextual-action-menu');
    Object.assign(actionMenu, {
      fileNum: 0,
      disabledTabIds: new Map(),
      tabSuggestions: [],
      smartTabSharingVisible: false,
    });
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
          menuTooltip: '',
        },
        {
          tool: ToolMode.kImageGen,
          menuLabel: 'Generate Image',
          disableActiveModelSelection: false,
          chipLabel: '',
          hintText: '',
          aimUrlParams: [],
          menuTooltip: '',
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
          menuTooltip: '',
        },
        {
          model: ModelMode.kGeminiPro,
          menuLabel: 'Gemini Pro',
          hintText: '',
          aimUrlParams: [],
          menuTooltip: '',
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
        'menuitemradio',
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
          menuTooltip: '',
        },
        {
          tool: ToolMode.kImageGen,
          menuLabel: 'Generate Image',
          disableActiveModelSelection: false,
          chipLabel: '',
          hintText: '',
          aimUrlParams: [],
          menuTooltip: '',
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
          menuTooltip: '',
        },
        {
          model: ModelMode.kGeminiPro,
          menuLabel: 'Gemini Pro',
          hintText: '',
          aimUrlParams: [],
          menuTooltip: '',
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
          menuTooltip: '',
        },
        {
          tool: ToolMode.kImageGen,
          menuLabel: 'Generate Image',
          disableActiveModelSelection: false,
          chipLabel: '',
          hintText: '',
          aimUrlParams: [],
          menuTooltip: '',
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
          menuTooltip: '',
        },
        {
          model: ModelMode.kGeminiPro,
          menuLabel: 'Gemini Pro',
          hintText: '',
          aimUrlParams: [],
          menuTooltip: '',
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
          menuTooltip: '',
        },
        {
          model: ModelMode.kGeminiPro,
          menuLabel: 'Gemini Pro',
          hintText: '',
          aimUrlParams: [],
          menuTooltip: '',
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

  test('Shows active tool checkmark and does not disable it', async () => {
    actionMenu.inputState = new MockInputState({
      allowedTools: [ToolMode.kDeepSearch, ToolMode.kImageGen],
      activeTool: ToolMode.kDeepSearch,
      disabledTools: [ToolMode.kDeepSearch],
      toolConfigs: [
        {
          tool: ToolMode.kDeepSearch,
          menuLabel: 'Deep Search',
          disableActiveModelSelection: false,
          chipLabel: '',
          hintText: '',
          aimUrlParams: [],
          menuTooltip: '',
        },
        {
          tool: ToolMode.kImageGen,
          menuLabel: 'Generate Image',
          disableActiveModelSelection: false,
          chipLabel: '',
          hintText: '',
          aimUrlParams: [],
          menuTooltip: '',
        },
      ],
      toolsSectionConfig: {header: ''},
      modelSectionConfig: {header: ''},
    });
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    const deepSearch =
        $$(actionMenu, `[data-mode="${ToolMode.kDeepSearch}"]`) as
        HTMLButtonElement;
    const imageGen =
        $$(actionMenu, `[data-mode="${ToolMode.kImageGen}"]`) as
        HTMLButtonElement;

    assertEquals('menuitemradio', deepSearch.getAttribute('role'));
    assertEquals('true', deepSearch.getAttribute('aria-checked'));

    assertEquals('menuitemradio', imageGen.getAttribute('role'));
    assertEquals('false', imageGen.getAttribute('aria-checked'));

    assertFalse(deepSearch.disabled);

    assertTrue(!!deepSearch.querySelector('cr-icon[icon="cr:check"]'));
    assertFalse(!!imageGen.querySelector('cr-icon[icon="cr:check"]'));
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
        menuTooltip: '',
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
        menuTooltip: '',
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
        menuTooltip: '',
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
        menuTooltip: '',
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
      url: 'about:blank',
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
      url: 'about:blank',
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

  test(
      'Browser tab suggestions disabled when input type disabled', async () => {
        // Arrange: Provide tab suggestions, allow browser tabs but also
        // disable them.
        const tabInfo = {
          tabId: 1,
          title: 'Google',
          url: 'about:blank',
          lastActiveTime: {internalValue: 0n},
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
        actionMenu.tabSuggestions = [tabInfo];
        actionMenu.inputState = new MockInputState({
          allowedInputTypes: [InputType.kBrowserTab],
          disabledInputTypes: [InputType.kBrowserTab],
          toolsSectionConfig: {header: ''},
          modelSectionConfig: {header: ''},
        });

        actionMenu.showAt(actionMenu);
        await microtasksFinished();

        // Assert: Tab suggestions should be shown (because allowed).
        const items = actionMenu.$.menu.querySelectorAll('.dropdown-item');
        assertEquals(1, items.length);

        const tabButton = items[0] as HTMLButtonElement;
        // And it should be disabled.
        assertTrue(tabButton.disabled);
      });

  test(
      'Browser tab suggestions disabled when they are thread restored',
      async () => {
        loadTimeData.overrideValues({
          contextManagementInComposeboxEnabled: true,
        });
        actionMenu.remove();
        actionMenu =
            document.createElement('cr-composebox-contextual-action-menu');

        const restoredTab = createTabSuggestion({
          tabId: 1,
          title: 'Restored Tab',
        });
        const suggestionTab = createTabSuggestion({
          tabId: 1,
          title: 'Restored Tab',
        });
        actionMenu.aimThreadRestoredTabs = [restoredTab];
        actionMenu.tabSuggestions = [suggestionTab];
        actionMenu.inputState = new MockInputState({
          allowedInputTypes: [InputType.kBrowserTab],
        });
        document.body.appendChild(actionMenu);
        await microtasksFinished();

        actionMenu.showAt(actionMenu);
        await microtasksFinished();

        const trigger = $$(actionMenu, '#shareTabsTrigger') as HTMLElement;
        const flyout = $$(actionMenu, '.share-tabs-flyout') as HTMLElement;
        assertTrue(!!trigger);
        assertTrue(!!flyout);

        // Hover to open flyout.
        trigger.dispatchEvent(new PointerEvent('pointerenter'));
        await microtasksFinished();

        const buttons = Array.from(
            flyout.querySelectorAll<HTMLButtonElement>('button.dropdown-item'));
        assertEquals(1, buttons.length);
        assertTrue(buttons[0]!.disabled);
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
        menuTooltip: '',
      }],
      toolsSectionConfig: {header: toolsHeader},
      allowedModels: [ModelMode.kGeminiRegular],
      modelConfigs: [{
        model: ModelMode.kGeminiRegular,
        menuLabel: geminiLabel,
        hintText: '',
        aimUrlParams: [],
        menuTooltip: '',
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
    loadTimeData.overrideValues({
      contextManagementInComposeboxEnabled: true,
    });
    actionMenu.remove();
    actionMenu = document.createElement('cr-composebox-contextual-action-menu');
    actionMenu.smartTabSharingVisible = true;
    actionMenu.tabSuggestions = [
      {
        tabId: 1,
        title: 'Tab 1',
        url: 'about:blank',
        showInCurrentTabChip: false,
        showInPreviousTabChip: false,
        lastActive: {internalValue: 0n},
      },
    ];
    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [InputType.kBrowserTab],
    });
    document.body.appendChild(actionMenu);

    actionMenu.smartTabSharingActive = true;
    actionMenu.showAt(actionMenu);
    await actionMenu.updateComplete;

    const item = $$(actionMenu, '#smartTabSharingItem');
    assertTrue(!!item);
    assertTrue(!!item.querySelector('.share-tabs-check'));
    assertEquals('true', item.getAttribute('aria-checked'));

    let eventDetail: {active: boolean}|null = null;
    actionMenu.addEventListener(
        'smart-tab-sharing-active-changed', (e: Event) => {
          eventDetail = (e as CustomEvent).detail;
        }, {once: true});

    item.click();

    assertTrue(!!eventDetail);
    assertFalse((eventDetail as {active: boolean}).active);
  });

  test('Clicking smart tab sharing row updates UI', async () => {
    loadTimeData.overrideValues({
      contextManagementInComposeboxEnabled: true,
    });
    actionMenu.remove();
    actionMenu = document.createElement('cr-composebox-contextual-action-menu');
    actionMenu.smartTabSharingVisible = true;
    actionMenu.tabSuggestions = [
      {
        tabId: 1,
        title: 'Tab 1',
        url: 'about:blank',
        showInCurrentTabChip: false,
        showInPreviousTabChip: false,
        lastActive: {internalValue: 0n},
      },
    ];
    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [InputType.kBrowserTab],
    });

    document.body.appendChild(actionMenu);

    actionMenu.smartTabSharingActive = true;
    actionMenu.showAt(actionMenu);
    await actionMenu.updateComplete;

    const item = $$(actionMenu, '#smartTabSharingItem');
    assertTrue(!!item);

    item.click();

    actionMenu.smartTabSharingActive = false;
    await actionMenu.updateComplete;

    const mainMenuToggle = $$(actionMenu, '#smartTabSharingItem');
    assertFalse(!!mainMenuToggle);

    const trigger = $$(actionMenu, '#shareTabsTrigger');
    assertTrue(!!trigger);
    assertTrue(isVisible(trigger));
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
      createTabSuggestion({
        tabId: 1,
        title: 'Tab 1',
      }),
    ];
    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [InputType.kBrowserTab],
    });
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

    // 50 suggestions to ensure content height exceeds window height.
    actionMenu.tabSuggestions = Array(50).fill({
      tabId: 1,
      title: 'Tab',
    });
    await microtasksFinished();

    // Ensure flyout max height scales to align with normal menu and fits in
    // viewport.
    const expectedMaxHeight = Math.max(
        MIN_MENU_HEIGHT_PX,
        Math.min(
            SHARE_TABS_FLYOUT_MAX_HEIGHT_PX,
            window.innerHeight - trigger.getBoundingClientRect().top -
                VIEWPORT_BUFFER_PX));
    assertEquals(expectedMaxHeight, flyout.offsetHeight);
  });

  test('Share tabs flyout repositions on scroll', async () => {
    loadTimeData.overrideValues({
      contextManagementInComposeboxEnabled: true,
    });

    actionMenu.remove();
    actionMenu = document.createElement('cr-composebox-contextual-action-menu');
    actionMenu.tabSuggestions = [
      createTabSuggestion({tabId: 1, title: 'Tab 1'}),
      createTabSuggestion({tabId: 2, title: 'Tab 2'}),
      createTabSuggestion({tabId: 3, title: 'Tab 3'}),
    ];

    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [InputType.kBrowserTab],
    });
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

    window.dispatchEvent(new Event('scroll'));
    await microtasksFinished();
    assertFalse(flyout.hidden);
  });

  test(
      'Constrain height if space below plus menu button is < menu height',
      async () => {
    // Arrange: Provide 20 tab suggestions to ensure height exceeds 540px.
    actionMenu.tabSuggestions = Array(20).fill(createTabSuggestion({
      tabId: 1,
      title: 'Tab Item',
    }));
    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [InputType.kBrowserTab],
      toolsSectionConfig: {header: ''},
      modelSectionConfig: {header: ''},
    });

    // Act.
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    // Assert: Main menu should be open and its height constrained to 540px (or less if viewport is small).
    const dialog = actionMenu.$.menu.getDialog();
    assertTrue(actionMenu.$.menu.open);

    const expectedMaxHeight =
        Math.min(DEFAULT_MAX_MENU_HEIGHT_PX, window.innerHeight - VIEWPORT_BUFFER_PX);
    assertEquals(expectedMaxHeight, dialog.offsetHeight);

    const style = window.getComputedStyle(dialog);
    assertEquals('auto', style.overflowY);
    assertTrue(dialog.scrollHeight > dialog.offsetHeight);
  });

  // TODO(crbug.com/512920161): Deflake and reenable this test.
  // <if expr="not is_linux and not is_macosx and not is_win and not is_chromeos">
  test('Share tabs flyout keyboard navigation', async () => {
    loadTimeData.overrideValues({
      contextManagementInComposeboxEnabled: true,
    });

    actionMenu.remove();
    actionMenu = document.createElement('cr-composebox-contextual-action-menu');
    actionMenu.tabSuggestions = [
      createTabSuggestion({
        tabId: 1,
        title: 'Tab 1',
      }),
    ];
    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [InputType.kBrowserTab],
    });
    document.body.appendChild(actionMenu);
    await microtasksFinished();

    // Open the main contextual action menu.
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    // Get the trigger button and the flyout container.
    const trigger = $$(actionMenu, '#shareTabsTrigger') as HTMLElement;
    const flyout = $$(actionMenu, '.share-tabs-flyout') as HTMLElement;
    assertTrue(!!trigger);
    assertTrue(!!flyout);
    // Verify that the flyout is hidden initially.
    assertTrue(flyout.hidden);

    triggerKeyDown(trigger, 'ArrowRight');
    await actionMenu.updateComplete;
    await new Promise(resolve => requestAnimationFrame(resolve));
    await microtasksFinished();

    // Assert that the flyout is now visible.
    assertFalse(flyout.hidden);

    // Assert that the keyboard focus has successfully moved to the first button
    // inside the flyout.
    const firstTabItem =
        flyout.querySelector<HTMLElement>('button.dropdown-item')!;

    assertTrue(!!firstTabItem);
    assertEquals(firstTabItem, actionMenu.shadowRoot.activeElement);

    triggerKeyDown(firstTabItem, 'ArrowLeft');
    await actionMenu.updateComplete;
    await new Promise(resolve => requestAnimationFrame(resolve));
    await microtasksFinished();

    assertTrue(flyout.hidden);

    // Assert that the focus is correctly returned to the parent trigger button.
    assertEquals(trigger, actionMenu.shadowRoot.activeElement);
  });

  test(
      'Share tabs flyout keyboard navigation focuses first non-disabled item',
      async () => {
        loadTimeData.overrideValues({
          contextManagementInComposeboxEnabled: true,
        });

        actionMenu.remove();
        actionMenu =
            document.createElement('cr-composebox-contextual-action-menu');
        const tab1 = createTabSuggestion({
          tabId: 1,
          title: 'Tab 1',
        });
        const tab2 = createTabSuggestion({
          tabId: 2,
          title: 'Tab 2',
        });

        actionMenu.tabSuggestions = [tab1, tab2];
        actionMenu.aimThreadRestoredTabs = [tab1];
        actionMenu.inputState = new MockInputState({
          allowedInputTypes: [InputType.kBrowserTab],
        });
        document.body.appendChild(actionMenu);
        await microtasksFinished();

        // Open the main contextual action menu.
        actionMenu.showAt(actionMenu);
        await microtasksFinished();

        // Get the trigger button and the flyout container.
        const trigger = $$(actionMenu, '#shareTabsTrigger') as HTMLElement;
        const flyout = $$(actionMenu, '.share-tabs-flyout') as HTMLElement;
        assertTrue(!!trigger);
        assertTrue(!!flyout);

        triggerKeyDown(trigger, 'ArrowRight');
        await actionMenu.updateComplete;
        await new Promise(resolve => requestAnimationFrame(resolve));
        await microtasksFinished();

        // Assert that the flyout is now visible.
        assertFalse(flyout.hidden);

        // Assert that the keyboard focus has successfully moved to the second
        // button inside the flyout, because the first is disabled.
        const buttons =
            flyout.querySelectorAll<HTMLButtonElement>('button.dropdown-item');
        assertEquals(2, buttons.length);
        assertTrue(buttons[0]!.disabled);
        assertFalse(buttons[1]!.disabled);

        assertEquals(buttons[1]!, actionMenu.shadowRoot.activeElement);
      });
  // </if>

  test('Tabs counter visibility', async () => {
    actionMenu.showAt(actionMenu);
    await microtasksFinished();
    assertFalse(!!$$(actionMenu, '#shareTabsTrigger'));

    // There is no tab counter if no tabs exist.
    loadTimeData.overrideValues({contextManagementInComposeboxEnabled: true});
    actionMenu.remove();
    actionMenu = document.createElement('cr-composebox-contextual-action-menu');
    actionMenu.tabSuggestions = [
      createTabSuggestion({
        tabId: 1,
        title: 'Tab 1',
      }),
    ];
    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [InputType.kBrowserTab],
      toolsSectionConfig: {header: ''},
      modelSectionConfig: {header: ''},
    });
    actionMenu.disabledTabIds = new Map();
    document.body.appendChild(actionMenu);
    actionMenu.showAt(actionMenu);
    await microtasksFinished();
    const shareTabsTrigger = $$(actionMenu, '#shareTabsTrigger');
    assertTrue(!!shareTabsTrigger);
    // The counter text should not be visible when no tabs are selected.
    assertFalse(shareTabsTrigger.textContent.includes('1'));

    // Show tab counter when one tab is chosen.
    actionMenu.disabledTabIds = new Map([[1, '1']]);
    await microtasksFinished();
    assertTrue(!!shareTabsTrigger.querySelector('.share-tabs-arrow'));

    // No tab counter when no tab is selected.
    actionMenu.disabledTabIds = new Map();
    await microtasksFinished();
    assertFalse(shareTabsTrigger.textContent.includes('1'));
  });

  test(
      'Tabs counter visibility with restored tabs and no suggestions',
      async () => {
        loadTimeData.overrideValues({
          contextManagementInComposeboxEnabled: true,
        });
        actionMenu.remove();
        actionMenu =
            document.createElement('cr-composebox-contextual-action-menu');
        const restoredTab = createTabSuggestion({
          tabId: 1,
          title: 'Restored Tab',
        });
        actionMenu.aimThreadRestoredTabs = [restoredTab];
        actionMenu.tabSuggestions = [restoredTab];
        actionMenu.inputState = new MockInputState({
          allowedInputTypes: [InputType.kBrowserTab],
          toolsSectionConfig: {header: ''},
          modelSectionConfig: {header: ''},
        });
        document.body.appendChild(actionMenu);
        actionMenu.showAt(actionMenu);
        await microtasksFinished();

        const shareTabsTrigger = $$(actionMenu, '#shareTabsTrigger');
        assertTrue(!!shareTabsTrigger);

        // Since we have restored tabs showing as suggestions, there should be a
        // dropdown arrow.
        assertTrue(!!shareTabsTrigger.querySelector('.share-tabs-arrow'));
      });


  test('Share tabs flyout cycling keyboard navigation', async () => {
    loadTimeData.overrideValues({
      contextManagementInComposeboxEnabled: true,
    });

    actionMenu.remove();
    actionMenu = document.createElement('cr-composebox-contextual-action-menu');
    const tab1 = createTabSuggestion({
      tabId: 1,
      title: 'Tab 1',
    });
    const tab2 = createTabSuggestion({
      tabId: 2,
      title: 'Tab 2',
    });

    actionMenu.tabSuggestions = [tab1, tab2];
    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [InputType.kBrowserTab],
    });
    document.body.appendChild(actionMenu);
    await microtasksFinished();

    // Open the main contextual action menu.
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    // Get the trigger button and the flyout container.
    const trigger = $$(actionMenu, '#shareTabsTrigger') as HTMLElement;
    const flyout = $$(actionMenu, '.share-tabs-flyout') as HTMLElement;
    assertTrue(!!trigger);
    assertTrue(!!flyout);

    // Expand the flyout.
    triggerKeyDown(trigger, 'ArrowRight');
    await actionMenu.updateComplete;
    await new Promise(resolve => requestAnimationFrame(resolve));
    await microtasksFinished();

    // Get the focusable items inside the flyout.
    const items = Array.from(
        flyout.querySelectorAll<HTMLElement>('button.dropdown-item'));
    assertEquals(2, items.length);

    const firstItem = items[0]!;
    const secondItem = items[1]!;

    // Focus the first item.
    firstItem.focus();
    assertEquals(firstItem, actionMenu.shadowRoot.activeElement);

    // Press ArrowDown to navigate to the second item.
    triggerKeyDown(firstItem, 'ArrowDown');
    await actionMenu.updateComplete;
    assertEquals(secondItem, actionMenu.shadowRoot.activeElement);

    // Press ArrowDown to navigate back to the first item (cycles through).
    triggerKeyDown(secondItem, 'ArrowDown');
    await actionMenu.updateComplete;
    assertEquals(firstItem, actionMenu.shadowRoot.activeElement);

    // Press ArrowUp to navigate back to the last item (cycles through).
    triggerKeyDown(firstItem, 'ArrowUp');
    await actionMenu.updateComplete;
    assertEquals(secondItem, actionMenu.shadowRoot.activeElement);
  });

  test('Share tabs flyout cycling skips disabled tabs', async () => {
    loadTimeData.overrideValues({
      contextManagementInComposeboxEnabled: true,
      composeboxFileMaxCount: 2,
    });

    actionMenu.remove();
    actionMenu = document.createElement('cr-composebox-contextual-action-menu');
    const tab1 = createTabSuggestion({
      tabId: 1,
      title: 'Tab 1',
    });
    const tab2 = createTabSuggestion({
      tabId: 2,
      title: 'Tab 2',
    });
    const tab3 = createTabSuggestion({
      tabId: 3,
      title: 'Tab 3',
    });
    const tab4 = createTabSuggestion({
      tabId: 4,
      title: 'Tab 4',
    });

    actionMenu.tabSuggestions = [tab1, tab2, tab3, tab4];
    // Select 2 tabs to reach the limit of 2, so that unselected tabs (2 & 4) are disabled.
    actionMenu.disabledTabIds = new Map([[1, 'uuid1'], [3, 'uuid3']]);
    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [InputType.kBrowserTab],
    });
    document.body.appendChild(actionMenu);
    await microtasksFinished();

    // Open the main contextual action menu.
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    // Get the trigger button and the flyout container.
    const trigger = $$(actionMenu, '#shareTabsTrigger') as HTMLElement;
    const flyout = $$(actionMenu, '.share-tabs-flyout') as HTMLElement;
    assertTrue(!!trigger);
    assertTrue(!!flyout);

    // Expand the flyout.
    triggerKeyDown(trigger, 'ArrowRight');
    await actionMenu.updateComplete;
    await new Promise(resolve => requestAnimationFrame(resolve));
    await microtasksFinished();

    // Get all items inside the flyout.
    const buttons = Array.from(
        flyout.querySelectorAll<HTMLButtonElement>('button.dropdown-item'));
    assertEquals(4, buttons.length);
    assertFalse(buttons[0]!.disabled);  // tab1 (selected) -> enabled
    assertTrue(buttons[1]!.disabled);   // tab2 (unselected) -> disabled due to limit
    assertFalse(buttons[2]!.disabled);  // tab3 (selected) -> enabled
    assertTrue(buttons[3]!.disabled);   // tab4 (unselected) -> disabled due to limit

    const firstItem = buttons[0]!;
    const thirdItem = buttons[2]!;

    // Focus the first enabled item (tab1).
    firstItem.focus();
    assertEquals(firstItem, actionMenu.shadowRoot.activeElement);

    // Press ArrowDown to navigate to the next enabled item (tab3), skipping
    // tab2.
    triggerKeyDown(firstItem, 'ArrowDown');
    await actionMenu.updateComplete;
    assertEquals(thirdItem, actionMenu.shadowRoot.activeElement);

    // Press ArrowDown to navigate/cycle back to the first enabled item (tab1),
    // skipping tab4.
    triggerKeyDown(thirdItem, 'ArrowDown');
    await actionMenu.updateComplete;
    assertEquals(firstItem, actionMenu.shadowRoot.activeElement);

    // Press ArrowUp to navigate/cycle back to the last enabled item (tab3),
    // skipping tab4.
    triggerKeyDown(firstItem, 'ArrowUp');
    await actionMenu.updateComplete;
    assertEquals(thirdItem, actionMenu.shadowRoot.activeElement);
  });

  test('focuses Share Tabs when opening the + menu via keydown', async () => {
    loadTimeData.overrideValues({
      contextManagementInComposeboxEnabled: true,
    });

    actionMenu.remove();
    actionMenu = document.createElement('cr-composebox-contextual-action-menu');

    // Initially, there is no tab data.
    actionMenu.tabSuggestions = [];
    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [InputType.kBrowserTab, InputType.kLensImage],
    });
    document.body.appendChild(actionMenu);
    await microtasksFinished();

    // Open the menu and wait for it to fully render.
    actionMenu.showAt(actionMenu);
    await actionMenu.updateComplete;
    await new Promise(resolve => setTimeout(resolve, 50));

    // Manually focus imageUpload to simulate the initial fallback state
    // where Share Tabs was missing.
    const imageUpload = $$(actionMenu, '#imageUpload') as HTMLElement;
    imageUpload.focus();
    assertEquals(imageUpload, actionMenu.shadowRoot.activeElement);

    // Simulate the asynchronous return of tab data from the backend.
    actionMenu.tabSuggestions = [
      createTabSuggestion({
        tabId: 1,
        title: 'Tab 1',
      }),
    ];

    await actionMenu.updateComplete;
    await new Promise(resolve => setTimeout(resolve, 50));

    // Assert that our updated logic successfully corrected the focus back to
    // Share Tabs.
    const trigger = $$(actionMenu, '#shareTabsTrigger') as HTMLElement;
    assertTrue(!!trigger);
    assertEquals(trigger, actionMenu.shadowRoot.activeElement);
  });

  test(
      'navigates up and down between Share Tabs and other menu items',
      async () => {
        loadTimeData.overrideValues({
          contextManagementInComposeboxEnabled: true,
        });

        actionMenu.remove();
        actionMenu =
            document.createElement('cr-composebox-contextual-action-menu');
        actionMenu.smartTabSharingVisible = true;

        actionMenu.smartTabSharingActive = true;
        actionMenu.tabSuggestions = [];
        actionMenu.inputState = new MockInputState({
          allowedInputTypes: [InputType.kBrowserTab, InputType.kLensImage],
        });
        document.body.appendChild(actionMenu);
        await microtasksFinished();

        actionMenu.showAt(actionMenu);
        await actionMenu.updateComplete;

        asInternal(actionMenu).onWindowBlur_ = () => {};

        const trigger = $$(actionMenu, '#smartTabSharingItem') as HTMLElement;
        const imageUpload = $$(actionMenu, '#imageUpload') as HTMLElement;

        await new Promise(resolve => requestAnimationFrame(resolve));

        trigger.focus();
        assertEquals(trigger, actionMenu.shadowRoot.activeElement);

        triggerKeyDown(trigger, 'ArrowDown');

        await microtasksFinished();

        assertEquals(imageUpload, actionMenu.shadowRoot.activeElement);
      });

  test('Share tabs flyout dynamic repositioning', async () => {
    loadTimeData.overrideValues({
      contextManagementInComposeboxEnabled: true,
    });

    actionMenu.remove();
    actionMenu = document.createElement('cr-composebox-contextual-action-menu');
    actionMenu.tabSuggestions = [
      createTabSuggestion({
        tabId: 1,
        title: 'Tab 1',
      }),
    ];
    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [InputType.kBrowserTab],
    });
    document.body.appendChild(actionMenu);
    await microtasksFinished();

    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    const trigger = $$(actionMenu, '#shareTabsTrigger') as HTMLElement;
    const flyout = $$(actionMenu, '.share-tabs-flyout') as HTMLElement;
    assertTrue(!!trigger);
    assertTrue(!!flyout);

    Object.defineProperty(flyout, 'offsetWidth', {value: 320, configurable: true});

    // Enough space to the right positions the flyout to the right.
    trigger.getBoundingClientRect = () => ({
      left: 10,
      right: 250,
      top: 100,
      bottom: 132,
      width: 240,
      height: 32,
    } as DOMRect);
    Object.defineProperty(window, 'innerWidth', {value: 1000, configurable: true});

    trigger.dispatchEvent(new PointerEvent('pointerenter'));
    await actionMenu.updateComplete;
    await microtasksFinished();

    assertEquals('right', flyout.getAttribute('data-position'));
    assertEquals('250px', flyout.style.left);

    // When blocked on the right, enough space to the left positions the flyout to the left.
    trigger.getBoundingClientRect = () => ({
      left: 400,
      right: 640,
      top: 100,
      bottom: 132,
      width: 240,
      height: 32,
    } as DOMRect);
    Object.defineProperty(window, 'innerWidth', {value: 800, configurable: true});

    trigger.dispatchEvent(new PointerEvent('pointerenter'));
    await actionMenu.updateComplete;
    await microtasksFinished();

    assertEquals('left', flyout.getAttribute('data-position'));
    assertEquals('80px', flyout.style.left);

    // When blocked on both sides in a narrow panel, the flyout positions at the bottom with a bounded indent.
    trigger.getBoundingClientRect = () => ({
      left: 16,
      right: 256,
      top: 100,
      bottom: 132,
      width: 240,
      height: 32,
    } as DOMRect);
    Object.defineProperty(window, 'innerWidth', {value: 380, configurable: true});

    trigger.dispatchEvent(new PointerEvent('pointerenter'));
    await actionMenu.updateComplete;
    await microtasksFinished();

    assertEquals('bottom', flyout.getAttribute('data-position'));
    assertEquals('16px', flyout.style.left);
  });

  test('Favicon group rendered in action menu', async () => {
    loadTimeData.overrideValues({
      contextManagementInComposeboxEnabled: true,
    });
    actionMenu.remove();
    actionMenu = document.createElement('cr-composebox-contextual-action-menu');
    const tabInfo = createTabSuggestion({
      tabId: 1,
      title: 'Tab 1',
    });
    actionMenu.tabSuggestions = [tabInfo];
    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [InputType.kBrowserTab],
    });
    actionMenu.disabledTabIds = new Map([[1, '1']]);
    document.body.appendChild(actionMenu);
    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    const faviconGroup =
        $$(actionMenu, 'composebox-favicon-group') as ComposeboxFaviconGroupElement;
    assertTrue(!!faviconGroup);
    assertEquals(1, faviconGroup.tabs.length);
  });

  test(
      'Disables uploads and tabs immediately when maxFileCount is reached',
      async () => {
        // Recreate actionMenu with maxFileCount = 1.
        actionMenu.remove();
        loadTimeData.overrideValues({
          composeboxFileMaxCount: 1,
        });
        actionMenu =
            document.createElement('cr-composebox-contextual-action-menu');
        actionMenu.fileNum = 1;  // Set fileNum to 1 (limit reached)

        // Provide tab suggestion.
        const tabInfo = createTabSuggestion({
          tabId: 1,
          title: 'Google',
        });
        actionMenu.tabSuggestions = [tabInfo];

        // inputState allows everything and disables nothing.
        actionMenu.inputState = new MockInputState({
          allowedInputTypes: [
            InputType.kLensImage,
            InputType.kLensFile,
            InputType.kDrive,
            InputType.kBrowserTab,
          ],
          disabledInputTypes: [],  // Nothing disabled by C++ yet
          toolsSectionConfig: {header: ''},
          modelSectionConfig: {header: ''},
        });

        document.body.appendChild(actionMenu);
        await microtasksFinished();

        actionMenu.showAt(actionMenu);
        await microtasksFinished();

        // Verify uploads are disabled.
        const imageUpload = $$(actionMenu, '#imageUpload') as HTMLButtonElement;
        const fileUpload = $$(actionMenu, '#fileUpload') as HTMLButtonElement;
        const driveUpload = $$(actionMenu, '#driveUpload') as HTMLButtonElement;

        assertTrue(imageUpload.disabled);
        assertTrue(fileUpload.disabled);
        assertTrue(driveUpload.disabled);

        // Verify tabs are disabled.
        const tabButton = actionMenu.$.menu.querySelector<HTMLButtonElement>(
            '.suggestion-container button')!;
        assertTrue(isVisible(tabButton));
        assertTrue(tabButton.disabled);
      });

  test(
      'Disables unselected tabs and enable selected tabs if max files reached',
      async () => {
        // Recreate actionMenu with maxFileCount = 1.
        actionMenu.remove();
        loadTimeData.overrideValues({
          composeboxFileMaxCount: 1,
        });
        actionMenu =
            document.createElement('cr-composebox-contextual-action-menu');
        actionMenu.fileNum = 1;  // Set fileNum to 1 (limit reached)

        // Provide 2 tab suggestions.
        const tabInfo1 = createTabSuggestion({
          tabId: 1,
          title: 'Google',
        });
        const tabInfo2 = createTabSuggestion({
          tabId: 2,
          title: 'YouTube',
        });
        actionMenu.tabSuggestions = [tabInfo1, tabInfo2];
        // Select tab 1.
        actionMenu.disabledTabIds = new Map([[1, 'uuid-1']]);

        // inputState allows everything and disables nothing.
        actionMenu.inputState = new MockInputState({
          allowedInputTypes: [
            InputType.kLensImage,
            InputType.kLensFile,
            InputType.kDrive,
            InputType.kBrowserTab,
          ],
          disabledInputTypes: [],
          toolsSectionConfig: {header: ''},
          modelSectionConfig: {header: ''},
        });

        document.body.appendChild(actionMenu);
        await microtasksFinished();

        actionMenu.showAt(actionMenu);
        await microtasksFinished();

        // Verify tabs items.
        const tabButtons =
            actionMenu.$.menu.querySelectorAll<HTMLButtonElement>(
                '.suggestion-container button');
        assertEquals(2, tabButtons.length);

        const tabButton1 = tabButtons[0]!;  // Tab 1 (Selected)
        const tabButton2 = tabButtons[1]!;  // Tab 2 (Unselected)

        // Tab 1 should be enabled so it can be deselected.
        assertFalse(tabButton1.disabled);

        // Tab 2 should be disabled since the limit is reached.
        assertTrue(tabButton2.disabled);
      });

  test('Disables all items when uploadButtonDisabled is true', async () => {
    actionMenu.uploadButtonDisabled = true;

    // Provide tab suggestion.
    const tabInfo = createTabSuggestion({
      tabId: 1,
      title: 'Google',
    });
    actionMenu.tabSuggestions = [tabInfo];

    // inputState allows everything and disables nothing.
    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [
        InputType.kLensImage,
        InputType.kLensFile,
        InputType.kDrive,
        InputType.kBrowserTab,
      ],
      allowedTools: [ToolMode.kDeepSearch],
      activeTool: ToolMode.kDeepSearch,
      toolConfigs: [{
        tool: ToolMode.kDeepSearch,
        menuLabel: 'Deep Search',
        disableActiveModelSelection: false,
        chipLabel: '',
        hintText: '',
        aimUrlParams: [],
        menuTooltip: '',
      }],
      toolsSectionConfig: {header: ''},
      allowedModels: [ModelMode.kGeminiRegular],
      modelConfigs: [{
        model: ModelMode.kGeminiRegular,
        menuLabel: 'Gemini Regular',
        hintText: '',
        aimUrlParams: [],
        menuTooltip: '',
      }],
      modelSectionConfig: {header: ''},
    });

    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    // Verify uploads are disabled.
    const imageUpload = $$(actionMenu, '#imageUpload') as HTMLButtonElement;
    const fileUpload = $$(actionMenu, '#fileUpload') as HTMLButtonElement;
    const driveUpload = $$(actionMenu, '#driveUpload') as HTMLButtonElement;
    assertTrue(imageUpload.disabled);
    assertTrue(fileUpload.disabled);
    assertTrue(driveUpload.disabled);

    // Verify tabs are disabled.
    const tabButton = actionMenu.$.menu.querySelector<HTMLButtonElement>(
        '.suggestion-container button')!;
    assertTrue(isVisible(tabButton));
    assertTrue(tabButton.disabled);

    // Verify tools are disabled.
    const deepSearch =
        $$(actionMenu, `[data-mode="${ToolMode.kDeepSearch}"]`) as
        HTMLButtonElement;
    assertTrue(deepSearch.disabled);

    // Verify models are disabled.
    const regularModel =
        $$(actionMenu, `[data-model="${ModelMode.kGeminiRegular}"]`) as
        HTMLButtonElement;
    assertTrue(regularModel.disabled);
  });

  test('Recent tab suffix disabled state', async () => {
    loadTimeData.overrideValues({
      contextManagementInComposeboxEnabled: true,
    });
    actionMenu.remove();
    actionMenu = document.createElement('cr-composebox-contextual-action-menu');
    const tabInfo = createTabSuggestion({
      tabId: 1,
      title: 'Recent Tab',
    });
    actionMenu.tabSuggestions = [tabInfo];
    actionMenu.recentTabId = tabInfo.tabId;
    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [InputType.kBrowserTab],
      disabledInputTypes: [InputType.kBrowserTab],
    });
    document.body.appendChild(actionMenu);
    await microtasksFinished();

    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    const trigger = $$(actionMenu, '#shareTabsTrigger') as HTMLElement;
    trigger.dispatchEvent(new PointerEvent('pointerenter'));
    await microtasksFinished();

    const suffix = $$(actionMenu, '.recent-tabs-suffix');
    assertTrue(isVisible(suffix));
    assertTrue(suffix!.hasAttribute('disabled'));
  });

  test('Share tabs trigger disabled when tab upload disabled', async () => {
    loadTimeData.overrideValues({
      contextManagementInComposeboxEnabled: true,
    });
    actionMenu.remove();
    actionMenu = document.createElement('cr-composebox-contextual-action-menu');
    const tabInfo: TabInfo = {
      tabId: 1,
      title: 'Recent Tab',
      url: 'about:blank',
      showInCurrentTabChip: false,
      showInPreviousTabChip: false,
      lastActive: {internalValue: 0n},
    };
    actionMenu.tabSuggestions = [tabInfo];
    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [InputType.kBrowserTab],
      disabledInputTypes: [InputType.kBrowserTab],
    });
    document.body.appendChild(actionMenu);
    await microtasksFinished();

    actionMenu.showAt(actionMenu);
    await microtasksFinished();

    const trigger = $$(actionMenu, '#shareTabsTrigger') as HTMLButtonElement;
    assertTrue(!!trigger);
    assertTrue(trigger.disabled);

    // Hovering should open the flyout.
    trigger.dispatchEvent(new PointerEvent('pointerenter'));
    await microtasksFinished();

    const flyout = $$(actionMenu, '.share-tabs-flyout') as HTMLElement;
    assertFalse(flyout.hidden);
  });

  test('Menu closes after tab selection in Realbox', async () => {
    loadTimeData.overrideValues({
      contextManagementInComposeboxEnabled: true,
      composeboxContextMenuEnableMultiTabSelection: true,
    });
    actionMenu.remove();
    actionMenu = document.createElement('cr-composebox-contextual-action-menu');
    Object.assign(actionMenu, {
      metricsSource_: 'NewTabPage',
    });

    const tabInfo = createTabSuggestion({
      tabId: 1,
      title: 'Tab 1',
    });
    actionMenu.tabSuggestions = [tabInfo];
    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [InputType.kBrowserTab],
    });
    document.body.appendChild(actionMenu);
    await microtasksFinished();

    actionMenu.showAt(actionMenu);
    Object.assign(actionMenu, {shareTabsFlyoutOpen_: true});
    await microtasksFinished();
    assertTrue(actionMenu.$.menu.open);

    const tabButton = actionMenu.$.menu.querySelector<HTMLButtonElement>(
        '.share-tabs-flyout button.dropdown-item')!;
    tabButton.click();
    await microtasksFinished();

    assertFalse(actionMenu.$.menu.open);
  });

  test(
      'Menu stays open after tab selection in Side Panel with multi-tab selection',
      async () => {
        loadTimeData.overrideValues({
          contextManagementInComposeboxEnabled: true,
          composeboxContextMenuEnableMultiTabSelection: true,
        });
        actionMenu.remove();
        actionMenu =
            document.createElement('cr-composebox-contextual-action-menu');
        Object.assign(actionMenu, {
          metricsSource_: 'contextual-tasks',
        });

        const tabInfo = createTabSuggestion({
          tabId: 1,
          title: 'Tab 1',
        });
        actionMenu.tabSuggestions = [tabInfo];
        actionMenu.inputState = new MockInputState({
          allowedInputTypes: [InputType.kBrowserTab],
        });
        document.body.appendChild(actionMenu);
        await microtasksFinished();

        actionMenu.showAt(actionMenu);
        Object.assign(actionMenu, {shareTabsFlyoutOpen_: true});
        await microtasksFinished();
        assertTrue(actionMenu.$.menu.open);

        const tabButton = actionMenu.$.menu.querySelector<HTMLButtonElement>(
            '.share-tabs-flyout button.dropdown-item')!;
        tabButton.click();
        await microtasksFinished();

        assertTrue(actionMenu.$.menu.open);
      });

  test(
      'Menu closes when a selected tab is clicked' +
          ' (deselected) in NTP/Omnibox mode',
      async () => {
        const tabInfo = createTabSuggestion({
          tabId: 1,
          title: 'Tab 1',
        });
        loadTimeData.overrideValues({
          contextManagementInComposeboxEnabled: true,
          composeboxContextMenuEnableMultiTabSelection: true,
        });
        actionMenu.remove();
        actionMenu =
            document.createElement('cr-composebox-contextual-action-menu');
        Object.assign(actionMenu, {
          metricsSource_: 'NewTabPage',
          disabledTabIds: new Map([[1, 'some-token']]),
          contextManagementInComposeboxEnabled: true,
        });

        actionMenu.tabSuggestions = [tabInfo];
        actionMenu.inputState = new MockInputState({
          allowedInputTypes: [InputType.kBrowserTab],
        });
        document.body.appendChild(actionMenu);
        await microtasksFinished();

        actionMenu.showAt(actionMenu);
        Object.assign(actionMenu, {shareTabsFlyoutOpen_: true});
        await microtasksFinished();
        assertTrue(actionMenu.$.menu.open);

        const tabButton = actionMenu.$.menu.querySelector<HTMLButtonElement>(
            '.share-tabs-flyout button.dropdown-item')!;
        tabButton.click();
        await microtasksFinished();

        assertFalse(actionMenu.$.menu.open);
      });

  test(
      'Recent tab suffix follows the correct tab after reordering',
      async () => {
        const tab1 = createTabSuggestion({
          tabId: 1,
          title: 'Tab 1',
          url: 'about:blank/1',
        });
        const tab2 = createTabSuggestion({
          tabId: 2,
          title: 'Tab 2',
          url: 'about:blank/2',
        });

        loadTimeData.overrideValues({
          contextManagementInComposeboxEnabled: true,
          composeboxContextMenuEnableMultiTabSelection: true,
        });
        actionMenu.remove();
        actionMenu =
            document.createElement('cr-composebox-contextual-action-menu');

        actionMenu.inputState = new MockInputState({
          allowedInputTypes: [InputType.kBrowserTab],
          toolsSectionConfig: {header: ''},
          modelSectionConfig: {header: ''},
        });

        // Backend initially provides Tab 1 as the first (most recent) item.
        actionMenu.tabSuggestions = [tab1, tab2];
        actionMenu.recentTabId = tab1.tabId;
        document.body.appendChild(actionMenu);

        actionMenu.showAt(actionMenu);
        await microtasksFinished();
        await actionMenu.updateComplete;

        // Precisely target only the tab items inside the Flyout.
        const getFlyoutItems = () => {
          return actionMenu.shadowRoot.querySelectorAll(
              '.share-tabs-flyout .dropdown-item');
        };

        let items = getFlyoutItems();
        assertEquals(2, items.length, 'The flyout menu should render 2 tabs');

        // Verify Tab 1 (index 0 in flyout) has the suffix and Tab 2 (index 1)
        // does not.
        assertTrue(
            !!items[0]?.querySelector('.recent-tabs-suffix'),
            'Tab 1 should have a suffix initially');
        assertFalse(
            !!items[1]?.querySelector('.recent-tabs-suffix'),
            'Tab 2 should not have a suffix initially');

        // Simulate frontend re-sorting (Tab 2 moved to index 0)
        actionMenu.tabSuggestions = [tab2, tab1];

        actionMenu.requestUpdate();
        await microtasksFinished();
        await actionMenu.updateComplete;

        // Allow a small amount of time for the Lit render tree to sync.
        await new Promise(resolve => setTimeout(resolve, 0));

        // Re-fetch the latest items inside the Flyout.
        items = getFlyoutItems();
        assertEquals(2, items.length);

        // The new index 0 (Tab 2) should NOT have the suffix.
        assertFalse(
            !!items[0]?.querySelector('.recent-tabs-suffix'),
            'Tab 2 should not have a suffix after reordering');

        // The suffix should still be on Tab 1, now at index 1.
        assertTrue(
            !!items[1]?.querySelector('.recent-tabs-suffix'),
            'Tab 1 should retain the suffix after reordering');
      });

  test(
      'Dynamic suffix shows Current Tab only in Side Panel Contextual Tasks',
      async () => {
        loadTimeData.overrideValues({
          contextManagementInComposeboxEnabled: true,
        });
        actionMenu.remove();
        actionMenu =
            document.createElement('cr-composebox-contextual-action-menu');

        const tabInfo = createTabSuggestion({
          tabId: 1,
          title: 'Google Docs',
        });
        actionMenu.tabSuggestions = [tabInfo];
        actionMenu.recentTabId = tabInfo.tabId;
        actionMenu.inputState = new MockInputState({
          allowedInputTypes: [InputType.kBrowserTab],
        });

        document.body.appendChild(actionMenu);
        await microtasksFinished();

        actionMenu.showAt(actionMenu);
        await microtasksFinished();

        const trigger = $$(actionMenu, '#shareTabsTrigger') as HTMLElement;
        trigger.dispatchEvent(new PointerEvent('pointerenter'));
        await microtasksFinished();

        const suffix = $$(actionMenu, '.recent-tabs-suffix') as HTMLElement;
        assertTrue(isVisible(suffix), 'Suffix should be visible');

        actionMenu.isSidePanel = true;
        await microtasksFinished();
        assertEquals(
            `· ${actionMenu.i18n('currentTabSuffix')}`,
            suffix.textContent.trim(),
            'Should render "Current tab" in side panel contextual tasks');

        actionMenu.isSidePanel = false;
        await microtasksFinished();
        assertEquals(
            `· ${actionMenu.i18n('recentTabsSuffix')}`,
            suffix.textContent.trim(),
            'Should fall back to "Recent tab" on the NTP');
      });

  suite('SmartTabSharingTogglePositioning', () => {
    setup(async () => {
      loadTimeData.overrideValues({
        contextManagementInComposeboxEnabled: true,
      });

      actionMenu.remove();
      actionMenu =
          document.createElement('cr-composebox-contextual-action-menu');
      actionMenu.smartTabSharingVisible = true;
      actionMenu.tabSuggestions = [
        createTabSuggestion({
          tabId: 1,
          title: 'Tab 1',
        }),
      ];
      actionMenu.inputState = new MockInputState({
        allowedInputTypes: [InputType.kBrowserTab],
      });
      document.body.appendChild(actionMenu);
      await microtasksFinished();
    });

    test('STS is OFF: Show Add tabs trigger, toggle in flyout', async () => {
      actionMenu.smartTabSharingActive = false;
      actionMenu.showAt(actionMenu);
      await microtasksFinished();
      await actionMenu.updateComplete;

      // Trigger is visible in main menu
      const trigger = $$(actionMenu, '#shareTabsTrigger');
      assertTrue(!!trigger);
      assertTrue(isVisible(trigger));

      // Main menu toggle is NOT visible
      const mainMenuToggle = $$(actionMenu, '#smartTabSharingItem');
      assertFalse(!!mainMenuToggle);

      // Open flyout
      trigger.dispatchEvent(new PointerEvent('pointerenter'));
      await microtasksFinished();
      await actionMenu.updateComplete;

      const flyout = $$(actionMenu, '.share-tabs-flyout');
      assertTrue(!!flyout);
      assertTrue(isVisible(flyout));

      // Toggle is visible in flyout
      const flyoutToggleItem = $$(actionMenu, '#smartTabSharingItemFlyout');
      assertTrue(!!flyoutToggleItem);
      assertTrue(isVisible(flyoutToggleItem));

      assertEquals('false', flyoutToggleItem.getAttribute('aria-checked'));
      assertFalse(!!flyoutToggleItem.querySelector('.share-tabs-check'));
    });

    test('STS is ON: Show toggle in main menu, no flyout', async () => {
      actionMenu.smartTabSharingActive = true;
      actionMenu.showAt(actionMenu);
      await microtasksFinished();
      await actionMenu.updateComplete;

      // Main menu toggle is visible
      const mainMenuToggle = $$(actionMenu, '#smartTabSharingItem');
      assertTrue(!!mainMenuToggle);
      assertTrue(isVisible(mainMenuToggle));

      assertEquals('true', mainMenuToggle.getAttribute('aria-checked'));
      assertTrue(!!mainMenuToggle.querySelector('.share-tabs-check'));
      // Trigger is NOT visible
      const trigger = $$(actionMenu, '#shareTabsTrigger');
      assertFalse(!!trigger);
    });

    test('Clicking toggle in flyout closes the menu', async () => {
      actionMenu.smartTabSharingActive = false;
      actionMenu.showAt(actionMenu);
      await microtasksFinished();
      await actionMenu.updateComplete;

      const trigger = $$(actionMenu, '#shareTabsTrigger');
      assertTrue(!!trigger);

      // Open flyout
      trigger.dispatchEvent(new PointerEvent('pointerenter'));
      await microtasksFinished();
      await actionMenu.updateComplete;

      const flyoutToggleItem =
          $$(actionMenu, '#smartTabSharingItemFlyout') as HTMLElement;
      assertTrue(!!flyoutToggleItem);

      // Verify menu is open
      assertTrue(actionMenu.$.menu.open);

      flyoutToggleItem.click();
      await microtasksFinished();

      // Verify menu is now closed!
      assertFalse(actionMenu.$.menu.open);
    });

    test('Clicking toggle in main menu does NOT close the menu', async () => {
      actionMenu.smartTabSharingActive = true;
      actionMenu.showAt(actionMenu);
      await microtasksFinished();
      await actionMenu.updateComplete;

      const mainMenuToggle =
          $$(actionMenu, '#smartTabSharingItem') as HTMLElement;
      assertTrue(!!mainMenuToggle);

      // Verify menu is open
      assertTrue(actionMenu.$.menu.open);

      mainMenuToggle.click();
      await microtasksFinished();

      // Verify menu stays open!
      assertTrue(actionMenu.$.menu.open);
    });

    test(
        'STS is ON: Show toggle even when suggestions are empty (prevent trapping)',
        async () => {
          actionMenu.smartTabSharingActive = true;
          actionMenu.tabSuggestions = [];
          actionMenu.showAt(actionMenu);
          await microtasksFinished();
          await actionMenu.updateComplete;

          const mainMenuToggle = $$(actionMenu, '#smartTabSharingItem');
          assertTrue(!!mainMenuToggle);
          assertTrue(isVisible(mainMenuToggle));

          const trigger = $$(actionMenu, '#shareTabsTrigger');
          assertFalse(!!trigger);
        });
  });

  suite('getSelectedTabs_', () => {
    test(
        'returns empty array when disabled and restored are empty', () => {
          actionMenu.disabledTabIds = new Map();
          actionMenu.aimThreadRestoredTabs = [];
          actionMenu.tabSuggestions = [
            createTabSuggestion({
              tabId: 1,
              title: 'Tab 1',
            }),
          ];
          const selectedTabs = asInternal(actionMenu).getSelectedTabs_();
          assertEquals(0, selectedTabs.length);
        });

    test(
        'returns matched tabs in reverse order of' +
            ' addition to disabled and concatenated with restored',
        () => {
          const tab1 = createTabSuggestion({
            tabId: 1,
            title: 'Tab 1',
          });
          const tab2 = createTabSuggestion({
            tabId: 2,
            title: 'Tab 2',
          });
          const tab3 = createTabSuggestion({
            tabId: 3,
            title: 'Tab 3',
          });

          actionMenu.tabSuggestions = [tab1, tab2, tab3];

          actionMenu.aimThreadRestoredTabs = [tab1];
          const disabledTabIds = new Map();
          disabledTabIds.set(2, 'token2');
          disabledTabIds.set(3, 'token3');
          actionMenu.disabledTabIds = disabledTabIds;

          const selectedTabs = asInternal(actionMenu).getSelectedTabs_();
          assertEquals(3, selectedTabs.length);
          // Given the displayed tabs are reversed (least to most recent),
          // tab3 should be first, then tab2, and restored tabs are concatenated
          // at the end (tab1).
          assertEquals(tab3, selectedTabs[0]);
          assertEquals(tab2, selectedTabs[1]);
          assertEquals(tab1, selectedTabs[2]);
        });

    test('filters out tab IDs not found in tabSuggestions', () => {
      const tab1 = createTabSuggestion({
        tabId: 1,
        title: 'Tab 1',
      });
      actionMenu.tabSuggestions = [tab1];

      actionMenu.aimThreadRestoredTabs = [];
      const disabledTabIds = new Map();
      disabledTabIds.set(1, 'token1');
      disabledTabIds.set(5, 'token5');
      actionMenu.disabledTabIds = disabledTabIds;

      const selectedTabs = asInternal(actionMenu).getSelectedTabs_();
      // Tab 5 is filtered out because it is not found in tabSuggestions.
      assertEquals(1, selectedTabs.length);
      assertEquals(tab1, selectedTabs[0]);
    });
  });

  suite('Positioning', () => {
    let anchor: HTMLButtonElement;
    let showAtCalls: any[] = [];
    let originalShowAt: any;

    setup(() => {
      anchor = document.createElement('button');
      document.body.appendChild(anchor);

      showAtCalls = [];
      originalShowAt = actionMenu.$.menu.showAt.bind(actionMenu.$.menu);
      actionMenu.$.menu.showAt = (_anchor: HTMLElement, options?: any) => {
        showAtCalls.push(options);
        originalShowAt(_anchor, options);
      };
    });

    teardown(() => {
      anchor.remove();
      actionMenu.$.menu.showAt = originalShowAt;
    });

    test('Anchors below the button if space below >= 160px', async () => {
      // Mock window innerHeight
      Object.defineProperty(window, 'innerHeight', {
        value: 800,
        configurable: true,
      });

      // Mock anchor position (rect.bottom = 500px, spaceBelow = 800 - 500 = 300px >= 160px)
      anchor.getBoundingClientRect = () => {
        return {
          bottom: 500,
          top: 450,
          left: 100,
          right: 200,
          width: 100,
          height: 50,
          x: 100,
          y: 450,
        } as DOMRect;
      };

      actionMenu.showAt(anchor);
      await microtasksFinished();

      // showAt is called twice: once for natural measurement, once for finalized positioning.
      assertEquals(2, showAtCalls.length);
      // The second call is the final positioning alignment.
      assertEquals(AnchorAlignment.AFTER_END, showAtCalls[1].anchorAlignmentY);
    });

    test('Renders underneath the button when in zero state', async () => {
      // Mock window innerHeight
      Object.defineProperty(window, 'innerHeight', {
        value: 800,
        configurable: true,
      });

      // Suggestions are empty (zero state).
      actionMenu.tabSuggestions = [];

      // Mock anchor position (rect.bottom = 200px, spaceBelow = 800 - 200 = 600px >= 160px)
      anchor.getBoundingClientRect = () => {
        return {
          bottom: 200,
          top: 150,
          left: 100,
          right: 200,
          width: 100,
          height: 50,
          x: 100,
          y: 150,
        } as DOMRect;
      };

      actionMenu.showAt(anchor);
      await microtasksFinished();

      const dialog = actionMenu.$.menu.getDialog();
      const dialogTop = dialog.getBoundingClientRect().top;
      // The top of the dialog should be >= the bottom of the button (200px)
      // to ensure it renders underneath the button.
      assertTrue(dialogTop >= 200);
    });

    test('Anchors above the button if space below < 160px', async () => {
      // Mock window innerHeight
      Object.defineProperty(window, 'innerHeight', {
        value: 600,
        configurable: true,
      });

      // Mock anchor position (rect.bottom = 500px, spaceBelow = 600 - 500 = 100px < 160px)
      anchor.getBoundingClientRect = () => {
        return {
          bottom: 500,
          top: 450,
          left: 100,
          right: 200,
          width: 100,
          height: 50,
          x: 100,
          y: 450,
        } as DOMRect;
      };

      actionMenu.showAt(anchor);
      await microtasksFinished();

      assertEquals(2, showAtCalls.length);
      assertEquals(AnchorAlignment.BEFORE_START, showAtCalls[1].anchorAlignmentY);
    });

    test(
        'Anchors to the right if space above and below are both < 362px',
        async () => {
          Object.defineProperty(actionMenu.$.menu.getDialog(), 'scrollHeight', {
            value: 380,
            configurable: true,
          });
          Object.defineProperty(window, 'innerHeight', {
            value: 500,
            configurable: true,
          });
          Object.defineProperty(window, 'innerWidth', {
            value: 1000,
            configurable: true,
          });

          anchor.getBoundingClientRect = () => {
            return {
              bottom: 300,
              top: 250,
              left: 100,
              right: 200,
              width: 100,
              height: 50,
              x: 100,
              y: 250,
            } as DOMRect;
          };

          actionMenu.showAt(anchor);
          await microtasksFinished();

          assertEquals(2, showAtCalls.length);
          assertEquals(
              AnchorAlignment.AFTER_END, showAtCalls[1].anchorAlignmentX);
          assertEquals(
              AnchorAlignment.AFTER_START, showAtCalls[1].anchorAlignmentY);
        });

    test(
        'Anchors to the right if space above is enough for menu but not for menu + buffer',
        async () => {
          Object.defineProperty(actionMenu.$.menu.getDialog(), 'scrollHeight', {
            value: 380,
            configurable: true,
          });
          Object.defineProperty(window, 'innerHeight', {
            value: 500,
            configurable: true,
          });
          Object.defineProperty(window, 'innerWidth', {
            value: 1000,
            configurable: true,
          });

          anchor.getBoundingClientRect = () => {
            return {
              bottom: 440,
              top: 390,
              left: 100,
              right: 200,
              width: 100,
              height: 50,
              x: 100,
              y: 390,
            } as DOMRect;
          };

          actionMenu.showAt(anchor);
          await microtasksFinished();

          assertEquals(2, showAtCalls.length);
          assertEquals(
              AnchorAlignment.AFTER_END, showAtCalls[1].anchorAlignmentX);
          assertEquals(
              AnchorAlignment.AFTER_START, showAtCalls[1].anchorAlignmentY);
        });

    test(
        'Anchors to the right of the icon even when favicon coins are present',
        async () => {
          Object.defineProperty(actionMenu.$.menu.getDialog(), 'scrollHeight', {
            value: 380,
            configurable: true,
          });
          Object.defineProperty(window, 'innerHeight', {
            value: 500,
            configurable: true,
          });
          Object.defineProperty(window, 'innerWidth', {
            value: 1000,
            configurable: true,
          });

          const mockIcon = document.createElement('div');
          mockIcon.id = 'entrypointIcon';
          mockIcon.getBoundingClientRect = () => {
            return {
              bottom: 290,
              top: 260,
              left: 100,
              right: 130,
              width: 30,
              height: 30,
              x: 100,
              y: 260,
            } as DOMRect;
          };
          anchor.appendChild(mockIcon);

          anchor.getBoundingClientRect = () => {
            return {
              bottom: 300,
              top: 250,
              left: 100,
              right: 250,
              width: 150,
              height: 50,
              x: 100,
              y: 250,
            } as DOMRect;
          };

          actionMenu.showAt(anchor);
          await microtasksFinished();

          assertEquals(2, showAtCalls.length);
          assertEquals(
              AnchorAlignment.AFTER_END, showAtCalls[1].anchorAlignmentX);
          assertEquals(
              AnchorAlignment.AFTER_START, showAtCalls[1].anchorAlignmentY);
          assertEquals(100, showAtCalls[1].left);
          assertEquals(30, showAtCalls[1].width);

          mockIcon.remove();
        });

    test(
        'Does not anchor to the right if obstructed by voice/lens buttons',
        async () => {
          Object.defineProperty(actionMenu.$.menu.getDialog(), 'scrollHeight', {
            value: 380,
            configurable: true,
          });
          const mockSearchbox = document.createElement('ntp-searchbox');
          const shadowRoot = mockSearchbox.attachShadow({mode: 'open'});

          const mockVoiceButton = document.createElement('button');
          mockVoiceButton.id = 'voiceSearchButton';
          mockVoiceButton.getBoundingClientRect = () => {
            return {
              left: 350,
              width: 40,
              height: 40,
              top: 255,
              bottom: 295,
            } as DOMRect;
          };
          shadowRoot.appendChild(mockVoiceButton);

          shadowRoot.appendChild(anchor);
          document.body.appendChild(mockSearchbox);

          Object.defineProperty(window, 'innerHeight', {
            value: 500,
            configurable: true,
          });
          Object.defineProperty(window, 'innerWidth', {
            value: 1000,
            configurable: true,
          });

          anchor.getBoundingClientRect = () => {
            return {
              bottom: 240,
              top: 190,
              left: 100,
              right: 200,
              width: 100,
              height: 50,
              x: 100,
              y: 190,
            } as DOMRect;
          };

          actionMenu.showAt(anchor);
          await microtasksFinished();

          mockSearchbox.remove();

          assertEquals(2, showAtCalls.length);
          assertEquals(
              AnchorAlignment.AFTER_START, showAtCalls[1].anchorAlignmentX);
          assertEquals(
              AnchorAlignment.AFTER_END, showAtCalls[1].anchorAlignmentY);
        });

    test('Does not overlap the button when anchoring from above',
          async () => {
      // Mock window innerHeight
      Object.defineProperty(window, 'innerHeight', {
        value: 450,
        configurable: true,
      });

      actionMenu.inputState = new MockInputState({
        allowedInputTypes: [InputType.kBrowserTab],
      });

      // Provide 10 tab suggestions so the natural height is larger than 354px.
      const origWidth = window.innerWidth;
      Object.defineProperty(window, 'innerWidth', {
        value: 300,
        configurable: true,
      });
      actionMenu.tabSuggestions = Array(10).fill({
        tabId: 1,
        title: 'Tab Item',
        url: {url: 'about:blank'},
        lastActiveTime: {internalValue: 0n},
        showInCurrentTabChip: false,
        showInPreviousTabChip: false,
        lastActive: {internalValue: 0n},
      });

      // Mock anchor position (rect.bottom = 410px, spaceBelow = 450
      // - 410 = 40px < 160px) spaceAbove = rect.top = 370px.
      // maxHeight = 370 - 16 (buffer) = 354px.
      anchor.getBoundingClientRect = () => {
        return {
          bottom: 410,
          top: 370,
          left: 100,
          right: 200,
          width: 100,
          height: 40,
          x: 100,
          y: 370,
        } as DOMRect;
      };

      await microtasksFinished();
      actionMenu.showAt(anchor);
      await microtasksFinished();

      const dialog = actionMenu.$.menu.getDialog();
      // The bottom of the dialog should be <= (higher than) the top of the
      // button (370px) to prevent overlap.
      const dialogBottom = dialog.getBoundingClientRect().bottom;
      assertTrue(dialogBottom <= 370);
      Object.defineProperty(window, 'innerWidth', {
        value: origWidth,
        configurable: true,
      });
    });

    test(
        'Does not overlap the button when suggestions load asynchronously after'
            + ' and grows the menu.',
        async () => {
          // Mock window innerHeight
          Object.defineProperty(window, 'innerHeight', {
            value: 450,
            configurable: true,
          });

          actionMenu.inputState = new MockInputState({
            allowedInputTypes: [InputType.kBrowserTab],
          });

          // Suggestions are initially empty.
          actionMenu.tabSuggestions = [];

          // Mock anchor position (rect.bottom = 410px, spaceBelow = 450
          // - 410 = 40px < 160px) spaceAbove = rect.top = 370px.
          // maxHeight = 370 - 16 (buffer) = 354px.
          anchor.getBoundingClientRect = () => {
            return {
              bottom: 410,
              top: 370,
              left: 100,
              right: 200,
              width: 100,
              height: 40,
              x: 100,
              y: 370,
            } as DOMRect;
          };

          const voiceButton = document.createElement('button');
          voiceButton.id = 'voiceSearchButton';
          voiceButton.getBoundingClientRect = () => ({
            bottom: 410,
            top: 370,
            left: 150,
            right: 200,
            width: 50,
            height: 40,
            x: 150,
            y: 370,
          } as DOMRect);
          document.body.appendChild(voiceButton);

          await microtasksFinished();
          actionMenu.showAt(anchor);
          await microtasksFinished();

          // Suggestions now load asynchronously.
          const repositionedPromise =
              eventToPromise('cr-action-menu-repositioned', actionMenu.$.menu);
          actionMenu.tabSuggestions = Array(10).fill({
            tabId: 1,
            title: 'Tab Item',
            url: {url: 'about:blank'},
            lastActiveTime: {internalValue: 0n},
            showInCurrentTabChip: false,
            showInPreviousTabChip: false,
            lastActive: {internalValue: 0n},
          });
          await Promise.all([microtasksFinished(), repositionedPromise]);

          const dialog = actionMenu.$.menu.getDialog();
          // The bottom of the dialog should be <= (higher than) the top of the
          // button (370px) to prevent overlap.
          const dialogBottom = dialog.getBoundingClientRect().bottom;
          assertTrue(dialogBottom <= 370);
          voiceButton.remove();
        });

    test(
        'Shows full menu for below, above, and right positions when space allows',
        async () => {
          Object.defineProperty(actionMenu.$.menu.getDialog(), 'scrollHeight', {
            value: 120,
            configurable: true,
          });

          // 1. Below position (spaceBelow = 800 - 500 = 300 >= 136)
          Object.defineProperty(window, 'innerHeight', {value: 800, configurable: true});
          anchor.getBoundingClientRect = () => ({
            bottom: 500, top: 450, left: 100, right: 200, width: 100, height: 50, x: 100, y: 450,
          } as DOMRect);

          actionMenu.showAt(anchor);
          await microtasksFinished();
          assertEquals(AnchorAlignment.AFTER_END, showAtCalls[showAtCalls.length - 1].anchorAlignmentY);
          assertEquals('284px', actionMenu.$.menu.style.getPropertyValue('--contextual-menu-max-height'));

          // 2. Above position (spaceAbove = 200 >= 136, spaceBelow = 300 - 250 = 50 < 136)
          Object.defineProperty(window, 'innerHeight', {value: 300, configurable: true});
          anchor.getBoundingClientRect = () => ({
            bottom: 250, top: 200, left: 100, right: 200, width: 100, height: 50, x: 100, y: 200,
          } as DOMRect);

          actionMenu.showAt(anchor);
          await microtasksFinished();
          assertEquals(AnchorAlignment.BEFORE_START, showAtCalls[showAtCalls.length - 1].anchorAlignmentY);
          assertEquals('184px', actionMenu.$.menu.style.getPropertyValue('--contextual-menu-max-height'));

          // 3. Right position (spaceAbove = 100 < 136, spaceBelow = 100 < 136, right vertical = 250 - 32 >= 120)
          Object.defineProperty(window, 'innerHeight', {value: 250, configurable: true});
          anchor.getBoundingClientRect = () => ({
            bottom: 150, top: 100, left: 100, right: 200, width: 100, height: 50, x: 100, y: 100,
          } as DOMRect);

          actionMenu.showAt(anchor);
          await microtasksFinished();
          assertEquals(AnchorAlignment.AFTER_END, showAtCalls[showAtCalls.length - 1].anchorAlignmentX);
          assertEquals(AnchorAlignment.AFTER_START, showAtCalls[showAtCalls.length - 1].anchorAlignmentY);
          assertEquals('', actionMenu.$.menu.style.getPropertyValue('--contextual-menu-max-height'));
        });

    test(
        'Reposition to right when suggestions load and it no longer fits vertically',
        async () => {
          // Mock window innerHeight and document clientHeight.
          Object.defineProperty(window, 'innerHeight', {
            value: 600,
            configurable: true,
          });
          Object.defineProperty(document.scrollingElement!, 'clientHeight', {
            value: 600,
            configurable: true,
          });

          // Empty suggestions initially.
          actionMenu.tabSuggestions = [];
          // Mock small scrollHeight for empty menu.
          Object.defineProperty(actionMenu.$.menu.getDialog(), 'scrollHeight', {
            value: 100,
            configurable: true,
          });

          actionMenu.inputState = new MockInputState({
            allowedInputTypes: [InputType.kBrowserTab],
          });

          // Anchor position: spaceBelow = 600 - 330 = 270. spaceAbove = 300.
          anchor.getBoundingClientRect = () => ({
            bottom: 330,
            top: 300,
            left: 100,
            right: 130,
            width: 30,
            height: 30,
            x: 100,
            y: 300,
          } as DOMRect);

          await microtasksFinished();
          // Act: Show the menu.
          actionMenu.showAt(anchor);
          await microtasksFinished();

          // Assert: It should be positioned below (since 100 + 16 < 270).
          assertEquals(2, showAtCalls.length);
          assertEquals(
              AnchorAlignment.AFTER_END, showAtCalls[1].anchorAlignmentY);
          assertEquals(
              AnchorAlignment.AFTER_START, showAtCalls[1].anchorAlignmentX);

          // Simulate suggestions loading.
          // Mock large scrollHeight and offsetHeight for populated menu.
          Object.defineProperty(actionMenu.$.menu.getDialog(), 'scrollHeight', {
            value: 400,
            configurable: true,
          });
          Object.defineProperty(actionMenu.$.menu.getDialog(), 'offsetHeight', {
            value: 400,
            configurable: true,
          });

          // Set suggestions to trigger update.
          const tab = createTabSuggestion({tabId: 1, title: 'Tab 1'});
          actionMenu.tabSuggestions = [tab];
          await actionMenu.updateComplete;
          await microtasksFinished();

          // Assert: It should have called showAt again.
          assertEquals(4, showAtCalls.length);
          // It should now anchor right because 400 + 16 > 270 (below) and 400 +
          // 16 > 300 (above).
          assertEquals(
              AnchorAlignment.AFTER_END, showAtCalls[3].anchorAlignmentX);
          assertEquals(
              AnchorAlignment.AFTER_START, showAtCalls[3].anchorAlignmentY);

          // Verify actual position (shifted up to fit in viewport).
          const dialog = actionMenu.$.menu.getDialog();
          assertEquals('184px', dialog.style.top);

          // Restore mocks.
          Reflect.deleteProperty(document.scrollingElement!, 'clientHeight');
        });

    test('Positioning is correct when document is scrolled', async () => {
      const spacer = document.createElement('div');
      spacer.style.height = '2000px';
      document.body.appendChild(spacer);

      window.scrollTo(0, 100);
      assertEquals(100, window.scrollY);

      Object.defineProperty(window, 'innerHeight', {
        value: 600,
        configurable: true,
      });
      Object.defineProperty(document.scrollingElement!, 'clientHeight', {
        value: 600,
        configurable: true,
      });

      Object.defineProperty(actionMenu.$.menu.getDialog(), 'scrollHeight', {
        value: 100,
        configurable: true,
      });
      Object.defineProperty(actionMenu.$.menu.getDialog(), 'offsetHeight', {
        value: 100,
        configurable: true,
      });

      anchor.getBoundingClientRect = () => ({
        bottom: 330,
        top: 300,
        left: 100,
        right: 200,
        width: 100,
        height: 30,
        x: 100,
        y: 300,
      } as DOMRect);

      actionMenu.showAt(anchor);
      await microtasksFinished();

      const dialog = actionMenu.$.menu.getDialog();
      const dialogRect = dialog.getBoundingClientRect();

      window.scrollTo(0, 0);
      spacer.remove();
      Reflect.deleteProperty(document.scrollingElement!, 'clientHeight');

      assertEquals(330, dialogRect.top);
    });
  });

  suite('ShareTabsFlyoutBehaviors', () => {
    setup(async () => {
      loadTimeData.overrideValues({
        contextManagementInComposeboxEnabled: true,
      });
      actionMenu.remove();
      actionMenu =
          document.createElement('cr-composebox-contextual-action-menu');
      actionMenu.tabSuggestions = [
        {
          tabId: 1,
          title: 'Tab 1',
          url: 'about:blank',
          lastActiveTime: {internalValue: 0n},
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        } as unknown as TabInfo,
      ];
      actionMenu.inputState = new MockInputState({
                                allowedInputTypes: [InputType.kBrowserTab],
                              }) as unknown as InputState;
      document.body.appendChild(actionMenu);
      await microtasksFinished();
    });

    test('shareTabFlyoutOpen changes based on events', async () => {
      actionMenu.showAt(actionMenu);
      await microtasksFinished();

      const trigger = $$(actionMenu, '#shareTabsTrigger') as HTMLElement;
      assertTrue(!!trigger);

      // row pointer enter sets true
      trigger.dispatchEvent(new PointerEvent('pointerenter'));
      await microtasksFinished();
      assertTrue(actionMenu.shareTabsFlyoutOpen);

      // arrow left sets false
      const flyout = $$(actionMenu, '.share-tabs-flyout') as HTMLElement;
      const firstTabItem =
          flyout.querySelector<HTMLElement>('button.dropdown-item');
      assertTrue(!!firstTabItem);
      firstTabItem.dispatchEvent(new KeyboardEvent(
          'keydown',
          {
            key: 'ArrowLeft',
            bubbles: true,
          },
          ));
      await microtasksFinished();
      assertFalse(actionMenu.shareTabsFlyoutOpen);

      // arrow right sets true
      trigger.dispatchEvent(
          new KeyboardEvent('keydown', {key: 'ArrowRight', bubbles: true}));
      await microtasksFinished();
      assertTrue(actionMenu.shareTabsFlyoutOpen);

      // resetShareTabsFlyout makes it false
      asInternal(actionMenu).resetShareTabsFlyout_();
      assertFalse(actionMenu.shareTabsFlyoutOpen);
    });

    test(
        'showAt calls updateFlyoutPosition if open, same with updated()',
        async () => {
          let updateFlyoutPositionCalled = false;
          asInternal(actionMenu).updateFlyoutPosition_ = () => {
            updateFlyoutPositionCalled = true;
          };

          actionMenu.shareTabsFlyoutOpen = true;
          actionMenu.showAt(actionMenu);
          assertTrue(updateFlyoutPositionCalled);

          updateFlyoutPositionCalled = false;
          // trigger updated
          actionMenu.tabSuggestions = [...actionMenu.tabSuggestions];
          await actionMenu.updateComplete;
          assertTrue(updateFlyoutPositionCalled);
        });

    test(
        'pointerLeave is ignored for 1 second after 1st tab added',
        async () => {
          asInternal(actionMenu).closeMenuOnSelect = false;
          actionMenu.showAt(actionMenu);
          await microtasksFinished();

          const trigger = $$(actionMenu, '#shareTabsTrigger') as HTMLElement;
          trigger.dispatchEvent(new PointerEvent('pointerenter'));
          await microtasksFinished();
          assertTrue(actionMenu.shareTabsFlyoutOpen);

          const flyout = $$(actionMenu, '.share-tabs-flyout') as HTMLElement;
          const tabButton =
              flyout.querySelector<HTMLElement>('button.dropdown-item')!;

          // Click tab to trigger 1 sec ignore
          tabButton.click();

          let scheduleCloseTimerCalled = false;
          asInternal(actionMenu).scheduleCloseTimer_ = () => {
            scheduleCloseTimerCalled = true;
          };

          // Pointer leave should be ignored
          trigger.dispatchEvent(new PointerEvent('pointerleave'));
          assertFalse(scheduleCloseTimerCalled);
        });

    test(
        'deleteTabContext, addTabContext close menu if ntp flag is off', () => {
          // Set to NTP source and flag off
          asInternal(actionMenu).metricsSource_ = 'NewTabPage';
          asInternal(actionMenu).closeMenuOnSelect =
              true;  // this means ntp flag is off

          actionMenu.showAt(actionMenu);
          assertTrue(actionMenu.$.menu.open);

          // test addTabContext
          asInternal(actionMenu).addTabContext_({
            tabId: 1,
            title: 'Test',
            url: 'about:blank',
          } as unknown as TabInfo);
          assertFalse(actionMenu.$.menu.open);

          actionMenu.showAt(actionMenu);
          assertTrue(actionMenu.$.menu.open);

          // test deleteTabContext
          asInternal(actionMenu).deleteTabContext_('0');
          assertFalse(actionMenu.$.menu.open);
        });
  });

  suite('ShareTabsFlyoutViewportPositioning', () => {
    let trigger: HTMLElement;
    let flyout: HTMLElement;

    const TRIGGER_WIDTH = 240;
    const TRIGGER_HEIGHT = 32;

    function createMockTriggerRect(left: number, top: number): DOMRect {
      return {
        left: left,
        right: left + TRIGGER_WIDTH,
        top: top,
        bottom: top + TRIGGER_HEIGHT,
        width: TRIGGER_WIDTH,
        height: TRIGGER_HEIGHT,
        x: left,
        y: top,
      } as DOMRect;
    }

    setup(async () => {
      loadTimeData.overrideValues({
        contextManagementInComposeboxEnabled: true,
      });
      actionMenu.remove();
      actionMenu =
          document.createElement('cr-composebox-contextual-action-menu');
      // Provide enough suggestions so the unconstrained content height is tall.
      actionMenu.tabSuggestions = Array(50).fill({
        tabId: 1,
        title: 'Tab',
        url: 'about:blank',
        lastActiveTime: {internalValue: 0n},
        showInCurrentTabChip: false,
        showInPreviousTabChip: false,
        lastActive: {internalValue: 0n},
      });
      actionMenu.inputState = new MockInputState({
        allowedInputTypes: [InputType.kBrowserTab],
      });
      document.body.appendChild(actionMenu);
      await microtasksFinished();

      actionMenu.showAt(actionMenu);
      await microtasksFinished();

      trigger = $$(actionMenu, '#shareTabsTrigger') as HTMLElement;
      flyout = $$(actionMenu, '.share-tabs-flyout') as HTMLElement;
      assertTrue(!!trigger);
      assertTrue(!!flyout);
    });

    test(
        'Positions flyout on the right when viewport width allows',
        async () => {
          const triggerLeft = 100;
          const triggerTop = 200;
          const viewportWidth = 1000;
          const viewportHeight = 800;

          Object.defineProperty(
              window, 'innerWidth', {value: viewportWidth, configurable: true});
          Object.defineProperty(
              window, 'innerHeight',
              {value: viewportHeight, configurable: true});
          Object.defineProperty(
              flyout, 'offsetWidth',
              {value: DEFAULT_FLYOUT_WIDTH_PX, configurable: true});

          trigger.getBoundingClientRect = () =>
              createMockTriggerRect(triggerLeft, triggerTop);

          trigger.dispatchEvent(new PointerEvent('pointerenter'));
          await microtasksFinished();

          const expectedLeft =
              `${triggerLeft + TRIGGER_WIDTH + SHARE_TABS_FLYOUT_GAP_PX}px`;
          const expectedTop = `${triggerTop}px`;
          const expectedMaxHeight = `${
              Math.max(
                  MIN_MENU_HEIGHT_PX,
                  Math.min(
                      SHARE_TABS_FLYOUT_MAX_HEIGHT_PX,
                      viewportHeight - triggerTop - VIEWPORT_BUFFER_PX))}px`;

          assertEquals(expectedLeft, flyout.style.left);
          assertEquals(expectedTop, flyout.style.top);
          assertEquals(expectedMaxHeight, flyout.style.maxHeight);
        });

    test(
        'Positions flyout to left when viewport is restricted on right',
        async () => {
          const triggerLeft = 350;
          const triggerTop = 150;
          const viewportWidth = 600;
          const viewportHeight = 700;

          Object.defineProperty(
              window, 'innerWidth', {value: viewportWidth, configurable: true});
          Object.defineProperty(
              window, 'innerHeight',
              {value: viewportHeight, configurable: true});
          Object.defineProperty(
              flyout, 'offsetWidth',
              {value: DEFAULT_FLYOUT_WIDTH_PX, configurable: true});

          trigger.getBoundingClientRect = () =>
              createMockTriggerRect(triggerLeft, triggerTop);

          trigger.dispatchEvent(new PointerEvent('pointerenter'));
          await microtasksFinished();

          const expectedLeft = `${
              triggerLeft - DEFAULT_FLYOUT_WIDTH_PX -
              SHARE_TABS_FLYOUT_GAP_PX}px`;
          const expectedTop = `${triggerTop}px`;
          const expectedMaxHeight = `${
              Math.max(
                  MIN_MENU_HEIGHT_PX,
                  Math.min(
                      SHARE_TABS_FLYOUT_MAX_HEIGHT_PX,
                      viewportHeight - triggerTop - VIEWPORT_BUFFER_PX))}px`;

          assertEquals(expectedLeft, flyout.style.left);
          assertEquals(expectedTop, flyout.style.top);
          assertEquals(expectedMaxHeight, flyout.style.maxHeight);
        });

    test(
        'Positions flyout to bottom when there is not enough viewport width',
        async () => {
          const triggerLeft = 100;
          const triggerTop = 100;
          const viewportWidth = 500;
          const viewportHeight = 600;

          Object.defineProperty(
              window, 'innerWidth', {value: viewportWidth, configurable: true});
          Object.defineProperty(
              window, 'innerHeight',
              {value: viewportHeight, configurable: true});
          Object.defineProperty(
              flyout, 'offsetWidth',
              {value: DEFAULT_FLYOUT_WIDTH_PX, configurable: true});

          trigger.getBoundingClientRect = () =>
              createMockTriggerRect(triggerLeft, triggerTop);

          trigger.dispatchEvent(new PointerEvent('pointerenter'));
          await microtasksFinished();

          const expectedLeft = `${triggerLeft}px`;
          const expectedTop =
              `${triggerTop + TRIGGER_HEIGHT + SHARE_TABS_FLYOUT_GAP_PX}px`;
          const expectedMaxHeight = `${
              Math.max(
                  MIN_MENU_HEIGHT_PX,
                  Math.min(
                      SHARE_TABS_FLYOUT_MAX_HEIGHT_PX,
                      viewportHeight -
                          (triggerTop + TRIGGER_HEIGHT +
                           SHARE_TABS_FLYOUT_GAP_PX) -
                          VIEWPORT_BUFFER_PX))}px`;

          assertEquals(expectedLeft, flyout.style.left);
          assertEquals(expectedTop, flyout.style.top);
          assertEquals(expectedMaxHeight, flyout.style.maxHeight);
        });

    test('Share tabs flyout scrollbar styles', async () => {
      // Arrange: Ensure tab suggestions exist so the flyout can be triggered.
      const tabInfo = createTabSuggestion({
        tabId: 101,
        title: 'Scrollbar Test Tab 1',
        url: 'about:blank/1',
      });
      actionMenu.tabSuggestions = Array(15).fill(tabInfo);
      actionMenu.inputState = new MockInputState({
        allowedInputTypes: [InputType.kBrowserTab],
      });

      // Act: Show the action menu and trigger the share tabs flyout open.
      actionMenu.showAt(actionMenu);
      await microtasksFinished();

      const trigger = $$(actionMenu, '#shareTabsTrigger') as HTMLElement;
      assertTrue(!!trigger);
      trigger.dispatchEvent(new PointerEvent('pointerenter'));
      await microtasksFinished();

      // Assert: Verify flyout container is rendered and properly styled.
      const flyout = $$(actionMenu, '.share-tabs-flyout') as HTMLElement;
      assertTrue(!!flyout);
      assertFalse(flyout.hidden);

      // Verify computed overflow and width styles match expectations.
      const computedStyle = window.getComputedStyle(flyout);
      assertEquals('auto', computedStyle.overflowY);
      assertEquals('hidden', computedStyle.overflowX);
      assertEquals('320px', computedStyle.width);

      // Verify the flyout does not have inline scrollbar-width override.
      assertFalse(flyout.style.getPropertyValue('scrollbar-width') === 'thin');
    });
  });
});

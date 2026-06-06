// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';
import 'chrome://resources/cr_components/composebox/contextual_action_menu.js';
import 'chrome://resources/cr_components/composebox/composebox_favicon_group.js';

import type {ComposeboxFaviconGroupElement} from 'chrome://resources/cr_components/composebox/composebox_favicon_group.js';
import type {ContextualActionMenuElement} from 'chrome://resources/cr_components/composebox/contextual_action_menu.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {InputType, ModelMode, ToolMode} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {$$, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {MockInputState} from './composebox_test_utils.js';

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
      {
        tabId: 1,
        title: 'Tab 1',
        url: {url: 'about:blank'},
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

    // 11 suggestions: 11 * 32px + 16px (padding) = 368px, which exceeds 344px
    // max height.
    actionMenu.tabSuggestions = Array(11).fill({
      tabId: 1,
      title: 'Tab',
      url: {url: 'about:blank'},
      lastActiveTime: {internalValue: 0n},
      showInCurrentTabChip: false,
      showInPreviousTabChip: false,
      lastActive: {internalValue: 0n},
    });
    await microtasksFinished();

    // Ensure flyout has max height even with many tab suggestions.
    assertEquals(344, flyout.offsetHeight);
  });

  test(
      'Constrain height if space below plus menu button is < menu height',
      async () => {
    // Arrange: Provide 20 tab suggestions to ensure height exceeds 540px.
    actionMenu.tabSuggestions = Array(20).fill({
      tabId: 1,
      title: 'Tab Item',
      url: {url: 'about:blank'},
      lastActiveTime: {internalValue: 0n},
      showInCurrentTabChip: false,
      showInPreviousTabChip: false,
      lastActive: {internalValue: 0n},
    });
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

    const expectedMaxHeight = Math.min(540, window.innerHeight - 16);
    assertEquals(expectedMaxHeight, dialog.offsetHeight);

    const style = window.getComputedStyle(dialog);
    assertEquals('visible', style.overflowY);
    assertTrue(dialog.scrollHeight > dialog.offsetHeight);
  });

  // TODO(crbug.com/512920161): Reenable this test on Linux and Mac and Windows
  // <if expr="not is_linux and not is_macosx and not is_win">
  test('Share tabs flyout keyboard navigation', async () => {
    loadTimeData.overrideValues({
      contextManagementInComposeboxEnabled: true,
    });

    actionMenu.remove();
    actionMenu = document.createElement('cr-composebox-contextual-action-menu');
    actionMenu.tabSuggestions = [
      {
        tabId: 1,
        title: 'Tab 1',
        url: {url: 'about:blank'},
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

    // Simulate an ArrowRight keydown event on the trigger to expand the flyout.
    trigger.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'ArrowRight', bubbles: true}));
    await actionMenu.updateComplete;
    await new Promise(resolve => requestAnimationFrame(resolve));
    await microtasksFinished();

    // Assert that the flyout is now visible.
    assertFalse(flyout.hidden);

    // Assert that the keyboard focus has successfully moved to the first button
    // inside the flyout.
    const firstTabItem =
        flyout.querySelector<HTMLElement>('button.dropdown-item');

    assertTrue(!!firstTabItem);
    assertEquals(firstTabItem, actionMenu.shadowRoot.activeElement);

    // Simulate an ArrowLeft keydown event on the inner item to collapse the
    // flyout.
    firstTabItem.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'ArrowLeft', bubbles: true}));
    await actionMenu.updateComplete;
    await new Promise(resolve => requestAnimationFrame(resolve));
    await microtasksFinished();

    assertTrue(flyout.hidden);

    // Assert that the focus is correctly returned to the parent trigger button.
    assertEquals(trigger, actionMenu.shadowRoot.activeElement);
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
      {
        tabId: 1,
        title: 'Tab 1',
        url: {url: 'about:blank'},
        lastActiveTime: {internalValue: 0n},
        showInCurrentTabChip: false,
        showInPreviousTabChip: false,
        lastActive: {internalValue: 0n},
      } as any,
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
        loadTimeData.overrideValues(
            {contextManagementInComposeboxEnabled: true});
        actionMenu.remove();
        actionMenu =
            document.createElement('cr-composebox-contextual-action-menu');
        const restoredTab: TabInfo = {
          tabId: 1,
          title: 'Restored Tab',
          url: 'about:blank',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
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

  test('focuses Share Tabs when opening the + menu via keydown', async () => {
    loadTimeData.overrideValues({
      contextManagementInComposeboxEnabled: true,
    });

    actionMenu.remove();
    actionMenu = document.createElement('cr-composebox-contextual-action-menu');

    // Initially, there is no tab data.
    actionMenu.tabSuggestions = [];
    actionMenu.inputState =
        new MockInputState({
          allowedInputTypes: [InputType.kBrowserTab, InputType.kLensImage],
        }) as any;
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
      {
        tabId: 1,
        title: 'Tab 1',
        url: {url: 'about:blank'},
        lastActiveTime: {internalValue: 0n},
        showInCurrentTabChip: false,
        showInPreviousTabChip: false,
        lastActive: {internalValue: 0n},
      } as any,
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
        actionMenu.inputState =
            new MockInputState({
              allowedInputTypes: [InputType.kBrowserTab, InputType.kLensImage],
            }) as any;
        document.body.appendChild(actionMenu);
        await microtasksFinished();

        actionMenu.showAt(actionMenu);
        await actionMenu.updateComplete;

        (actionMenu as any).onWindowBlur_ = () => {};

        const trigger = $$(actionMenu, '#smartTabSharingItem') as HTMLElement;
        const imageUpload = $$(actionMenu, '#imageUpload') as HTMLElement;

        await new Promise(resolve => requestAnimationFrame(resolve));

        trigger.focus();
        assertEquals(trigger, actionMenu.shadowRoot.activeElement);

        trigger.dispatchEvent(new KeyboardEvent('keydown', {
          key: 'ArrowDown',
          code: 'ArrowDown',
          keyCode: 40,
          bubbles: true,
          composed: true,
          cancelable: true,
        } as any));

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
      {
        tabId: 1,
        title: 'Tab 1',
        url: {url: 'about:blank'},
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
    assertEquals('', flyout.style.left);

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
    assertEquals('', flyout.style.left);

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
    // The expected maxLeft.
    assertEquals('32px', flyout.style.left);
  });

  test('Favicon group rendered in action menu', async () => {
    loadTimeData.overrideValues({ contextManagementInComposeboxEnabled: true });
    actionMenu.remove();
    actionMenu = document.createElement('cr-composebox-contextual-action-menu');
    const tabInfo: TabInfo = {
      tabId: 1,
      title: 'Tab 1',
      url: 'about:blank',
      showInCurrentTabChip: false,
      showInPreviousTabChip: false,
      lastActive: {internalValue: 0n},
    };
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

  test('Disables all items when uploadButtonDisabled is true', async () => {
    actionMenu.uploadButtonDisabled = true;

    // Provide tab suggestion.
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

    // inputState allows everything and disables nothing.
    actionMenu.inputState = new MockInputState({
      allowedInputTypes: [
        InputType.kLensImage,
        InputType.kLensFile,
        InputType.kDrive,
        InputType.kBrowserTab,
      ],
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
      allowedModels: [ModelMode.kGeminiRegular],
      modelConfigs: [{
        model: ModelMode.kGeminiRegular,
        menuLabel: 'Gemini Regular',
        hintText: '',
        aimUrlParams: [],
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
    const tabInfo: TabInfo = {
      tabId: 1,
      title: 'Recent Tab',
      url: 'about:blank',
      showInCurrentTabChip: false,
      showInPreviousTabChip: false,
      lastActive: {internalValue: 0n},
    };
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

    const tabInfo: TabInfo = {
      tabId: 1,
      title: 'Tab 1',
      url: 'about:blank',
      showInCurrentTabChip: false,
      showInPreviousTabChip: false,
      lastActive: {internalValue: 0n},
    };
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

        const tabInfo: TabInfo = {
          tabId: 1,
          title: 'Tab 1',
          url: 'about:blank',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
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
    'Menu closes when a selected tab is clicked (deselected) in NTP/Omnibox mode',
    async () => {
      const tabInfo: TabInfo = {
        tabId: 1,
        title: 'Tab 1',
        url: 'about:blank',
        showInCurrentTabChip: false,
        showInPreviousTabChip: false,
        lastActive: { internalValue: 0n },
      };
      loadTimeData.overrideValues({
        contextManagementInComposeboxEnabled: true,
        composeboxContextMenuEnableMultiTabSelection: true,
      });
      actionMenu.remove();
      actionMenu = document.createElement('cr-composebox-contextual-action-menu');
      Object.assign(actionMenu, {
        metricsSource_: 'NewTabPage',
        disabledTabIds: new Map([[1, 'some-token']]),
      });

      actionMenu.tabSuggestions = [tabInfo];
      actionMenu.inputState = new MockInputState({
        allowedInputTypes: [InputType.kBrowserTab],
      });
      document.body.appendChild(actionMenu);
      await microtasksFinished();

      actionMenu.showAt(actionMenu);
      Object.assign(actionMenu, { shareTabsFlyoutOpen_: true });
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
        const tab1: TabInfo = {
          tabId: 1,
          title: 'Tab 1',
          url: 'about:blank/1',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
        const tab2: TabInfo = {
          tabId: 2,
          title: 'Tab 2',
          url: 'about:blank/2',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };

        actionMenu['contextManagementInComposeboxEnabled_'] = true;

        actionMenu.inputState = new MockInputState({
          allowedInputTypes: [InputType.kBrowserTab],
          toolsSectionConfig: {header: ''},
          modelSectionConfig: {header: ''},
        });

        // Backend initially provides Tab 1 as the first (most recent) item.
        actionMenu.tabSuggestions = [tab1, tab2];
        actionMenu.recentTabId = tab1.tabId;

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

        const tabInfo = {
          tabId: 1,
          title: 'Google Docs',
          url: 'about:blank',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
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
            actionMenu.i18n('currentTabSuffix'), suffix.textContent.trim(),
            'Should render "Current tab" in side panel contextual tasks');

        actionMenu.isSidePanel = false;
        await microtasksFinished();
        assertEquals(
            actionMenu.i18n('recentTabsSuffix'), suffix.textContent.trim(),
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
            {
              tabId: 1,
              title: 'Tab 1',
              url: 'about:blank',
              showInCurrentTabChip: false,
              showInPreviousTabChip: false,
              lastActive: {internalValue: 0n},
            },
          ];
          const selectedTabs = (actionMenu as any).getSelectedTabs_();
          assertEquals(0, selectedTabs.length);
        });

    test(
        'returns matched tabs in reverse order of' +
            ' addition to disabled and concatenated with restored',
        () => {
          const tab1: TabInfo = {
            tabId: 1,
            title: 'Tab 1',
            url: 'about:blank',
            showInCurrentTabChip: false,
            showInPreviousTabChip: false,
            lastActive: {internalValue: 0n},
          };
          const tab2: TabInfo = {
            tabId: 2,
            title: 'Tab 2',
            url: 'about:blank',
            showInCurrentTabChip: false,
            showInPreviousTabChip: false,
            lastActive: {internalValue: 0n},
          };
          const tab3: TabInfo = {
            tabId: 3,
            title: 'Tab 3',
            url: 'about:blank',
            showInCurrentTabChip: false,
            showInPreviousTabChip: false,
            lastActive: {internalValue: 0n},
          };

          actionMenu.tabSuggestions = [tab1, tab2, tab3];

          actionMenu.aimThreadRestoredTabs = [tab1];
          const disabledTabIds = new Map();
          disabledTabIds.set(2, 'token2');
          disabledTabIds.set(3, 'token3');
          actionMenu.disabledTabIds = disabledTabIds;

          const selectedTabs = (actionMenu as any).getSelectedTabs_();
          assertEquals(3, selectedTabs.length);
          // Given the displayed tabs are reversed (least to most recent),
          // tab3 should be first, then tab2, and restored tabs are concatenated
          // at the end (tab1).
          assertEquals(tab3, selectedTabs[0]);
          assertEquals(tab2, selectedTabs[1]);
          assertEquals(tab1, selectedTabs[2]);
        });

    test('filters out tab IDs not found in tabSuggestions', () => {
      const tab1: TabInfo = {
        tabId: 1,
        title: 'Tab 1',
        url: 'about:blank',
        showInCurrentTabChip: false,
        showInPreviousTabChip: false,
        lastActive: {internalValue: 0n},
      };
      actionMenu.tabSuggestions = [tab1];

      actionMenu.aimThreadRestoredTabs = [];
      const disabledTabIds = new Map();
      disabledTabIds.set(1, 'token1');
      disabledTabIds.set(5, 'token5');
      actionMenu.disabledTabIds = disabledTabIds;

      const selectedTabs = (actionMenu as any).getSelectedTabs_();
      // Tab 5 is filtered out because it is not found in tabSuggestions.
      assertEquals(1, selectedTabs.length);
      assertEquals(tab1, selectedTabs[0]);
    });
  });
});

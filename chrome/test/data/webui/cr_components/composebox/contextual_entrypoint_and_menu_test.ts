// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/strings.m.js';
import 'chrome://resources/cr_components/composebox/contextual_entrypoint_and_menu.js';

import type {ContextualEntrypointAndMenuElement} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import type {ContextualEntrypointButtonElement} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_button.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ToolMode} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {InputState} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createValidInputState} from './composebox_test_utils.js';

suite('ContextualEntrypointAndMenu', () => {
  let entrypointAndMenu: ContextualEntrypointAndMenuElement;

  async function createComponent(
      inputState: InputState|null = createValidInputState()) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    entrypointAndMenu =
        document.createElement('cr-composebox-contextual-entrypoint-and-menu');
    Object.assign(entrypointAndMenu, {
      inputState,
      usePecApi: loadTimeData.getBoolean('contextualMenuUsePecApi'),
      tabSuggestionsHasLoaded: true,
    });
    document.body.appendChild(entrypointAndMenu);
    await microtasksFinished();
  }

  setup(async () => {
    loadTimeData.overrideValues({
      composeboxContextMenuEnableMultiTabSelection: true,
      contextualMenuUsePecApi: true,
      composeboxShowContextMenuDescription: true,
    });
    await createComponent();
  });

  test('context menu description hides in tool mode', async () => {
    // Initial state: not in tool mode. Description should be shown.
    let entrypointButton =
        $$(entrypointAndMenu, '#entrypointButton') as ContextualEntrypointButtonElement;
    assertTrue(!!entrypointButton);
    assertTrue(entrypointButton.showContextMenuDescription);

    // Enter tool mode.
    entrypointButton.inputState = {
      ...createValidInputState(),
      activeTool: ToolMode.kDeepSearch,
    };
    await microtasksFinished();

    // In tool mode. Description should be hidden.
    entrypointButton =
        $$(entrypointAndMenu, '#entrypointButton') as ContextualEntrypointButtonElement;
    assertTrue(!!entrypointButton);
    assertFalse(entrypointButton.showContextMenuDescription);

    // Exit tool mode.
    entrypointAndMenu.inputState = {
      ...createValidInputState(),
      activeTool: ToolMode.kUnspecified,
    };
    await microtasksFinished();

    // Back to unspecified mode. Description should be shown again.
    entrypointButton =
        $$(entrypointAndMenu, '#entrypointButton') as ContextualEntrypointButtonElement;
    assertTrue(!!entrypointButton);
    assertTrue(entrypointButton.showContextMenuDescription);
  });

  test('context menu shown on entrypoint click event', async () => {
    const openedPromise =
        eventToPromise('context-menu-opened', entrypointAndMenu);
    // Act.
    $$(entrypointAndMenu, '#entrypointButton')!.dispatchEvent(
        new Event('context-menu-entrypoint-click'));
    await openedPromise;

    // Assert.
    assertTrue(entrypointAndMenu.$.menu.open);
  });

  test('entrypoint button is displayed when not using pec api', async () => {
    loadTimeData.overrideValues({
      contextualMenuUsePecApi: false,
    });
    await createComponent(/* inputState= */ null);

    const entrypointButton = $$(entrypointAndMenu, '#entrypointButton');

    assertTrue(!!entrypointButton);
  });

  test('entrypoint button is displayed with valid input state', () => {
    const entrypoint = $$(entrypointAndMenu, '#entrypointButton');

    assertTrue(!!entrypoint);
  });

  test('entrypoint button is hidden with invalid input state', async () => {
    entrypointAndMenu.inputState = null;
    await microtasksFinished();

    const entrypoint = $$(entrypointAndMenu, '#entrypointButton');

    assertFalse(!!entrypoint);
  });

  test('menu does not open if entrypoint button is hidden', async () => {
    entrypointAndMenu.inputState = null;
    await microtasksFinished();

    assertFalse(!!$$(entrypointAndMenu, '#entrypointButton'));

    // Attempt to open the menu.
    entrypointAndMenu.openMenuForMultiSelection();
    await microtasksFinished();

    // The menu should remain closed when the button is hidden.
    assertFalse(entrypointAndMenu.$.menu.open);
  });

  test('menu opens asynchronously once input state becomes valid', async () => {
    entrypointAndMenu.inputState = null;
    await microtasksFinished();

    // Attempt to open the menu.
    entrypointAndMenu.openMenuForMultiSelection();
    await microtasksFinished();

    // The menu should remain closed when the button is hidden.
    assertFalse(entrypointAndMenu.$.menu.open);

    // Provide a valid inputState.
    entrypointAndMenu.inputState = createValidInputState();
    await microtasksFinished();

    // The menu should now open.
    assertTrue(entrypointAndMenu.$.menu.open);
  });

  test(
      'invalid input state clears pending menu open request',
      async () => {
        entrypointAndMenu.inputState = null;
        await microtasksFinished();

        assertFalse(!!$$(entrypointAndMenu, '#entrypointButton'));

        // Attempt to open the menu.
        entrypointAndMenu.openMenuForMultiSelection();
        await microtasksFinished();

        // The menu should remain closed when the button is hidden.
        assertFalse(entrypointAndMenu.$.menu.open);

        // Provide an invalid inputState by clearing the allowed inputs, tools,
        // and models.
        entrypointAndMenu.inputState = {
          ...createValidInputState(),
          allowedInputTypes: [],
          allowedTools: [],
          allowedModels: [],
        };
        await microtasksFinished();

        // Provide a valid inputState.
        entrypointAndMenu.inputState = createValidInputState();
        await microtasksFinished();

        // The menu should remain closed because the pending open request was
        // cleared.
        assertFalse(entrypointAndMenu.$.menu.open);
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

  test(
      'context menu does not open if multi-tab selection is disabled',
      async () => {
        loadTimeData.overrideValues({
          composeboxContextMenuEnableMultiTabSelection: false,
        });
        await createComponent();

        entrypointAndMenu.openMenuForMultiSelection();
        await microtasksFinished();

        assertFalse(entrypointAndMenu.$.menu.open);
      });

  test('DisableAutoRepositionForwardsToInnerMenu', async () => {
    entrypointAndMenu.disableAutoReposition = true;
    await entrypointAndMenu.updateComplete;

    const innerActionMenu = entrypointAndMenu.$.menu;
    assertTrue(innerActionMenu.disableAutoReposition);

    await innerActionMenu.updateComplete;
    assertFalse(innerActionMenu.$.menu.hasAttribute('auto-reposition'));
    assertFalse(innerActionMenu.$.menu.autoReposition);
  });

  test('DefaultPathPreservesAutoRepositionThroughWrapperChain', async () => {
    const innerActionMenu = entrypointAndMenu.$.menu;
    await entrypointAndMenu.updateComplete;
    assertFalse(innerActionMenu.disableAutoReposition);
    assertTrue(innerActionMenu.$.menu.hasAttribute('auto-reposition'));
    assertTrue(innerActionMenu.$.menu.autoReposition);
  });

  suite('aimThreadRestoredTabs', () => {
    test(
        'forwards aimThreadRestoredTabs to entrypoint button and action menu',
        async () => {
          const restoredTabs: TabInfo[] = [
            {
              tabId: 1,
              title: 'Tab 1',
              url: 'about:blank?1',
              showInCurrentTabChip: false,
              showInPreviousTabChip: false,
              lastActive: {internalValue: 0n},
            },
            {
              tabId: 2,
              title: 'Tab 2',
              url: 'about:blank?2',
              showInCurrentTabChip: false,
              showInPreviousTabChip: false,
              lastActive: {internalValue: 0n},
            },
          ];

          entrypointAndMenu.aimThreadRestoredTabs = restoredTabs;
          await entrypointAndMenu.updateComplete;

          const entrypointButton = $$(entrypointAndMenu, '#entrypointButton') as
              ContextualEntrypointButtonElement;
          assertTrue(!!entrypointButton);
          assertEquals(restoredTabs, entrypointButton.restoredTabs);

          const menu = entrypointAndMenu.$.menu;
          assertTrue(!!menu);
          assertEquals(restoredTabs, menu.aimThreadRestoredTabs);
        });
  });

  suite('SuggestionsLoadingDelay', () => {
    test(
        'click when not loaded and not loading requests load and delays opening',
        async () => {
          entrypointAndMenu.tabSuggestionsHasLoaded = false;
          entrypointAndMenu.tabSuggestionsLoading = false;
          entrypointAndMenu.menuOpenDelayMs = 1000;
          await microtasksFinished();

          let requestedLoad = false;
          entrypointAndMenu.addEventListener(
              'request-tab-suggestions-load', () => {
                requestedLoad = true;
              });

          const button = $$(entrypointAndMenu, '#entrypointButton')!;
          button.dispatchEvent(new Event('context-menu-entrypoint-click'));
          await microtasksFinished();

          assertTrue(requestedLoad);
          assertTrue(entrypointAndMenu.tabSuggestionsLoading);
          assertFalse(entrypointAndMenu.$.menu.open);

          const openedPromise =
              eventToPromise('context-menu-opened', entrypointAndMenu);
          entrypointAndMenu.tabSuggestionsHasLoaded = true;
          entrypointAndMenu.tabSuggestionsLoading = false;
          await openedPromise;

          assertTrue(entrypointAndMenu.$.menu.open);
        });

    test('menu opening is delayed when suggestions are loading', async () => {
      entrypointAndMenu.tabSuggestionsLoading = true;
      entrypointAndMenu.menuOpenDelayMs = 1000;
      await microtasksFinished();

      const button = $$(entrypointAndMenu, '#entrypointButton')!;
      button.dispatchEvent(new Event('context-menu-entrypoint-click'));
      await microtasksFinished();

      assertFalse(entrypointAndMenu.$.menu.open);

      const openedPromise =
          eventToPromise('context-menu-opened', entrypointAndMenu);
      entrypointAndMenu.tabSuggestionsLoading = false;
      await openedPromise;

      assertTrue(entrypointAndMenu.$.menu.open);
    });

    test(
        'menu opening fallback opens after timeout if loading takes too long',
        async () => {
          const openedPromise =
              eventToPromise('context-menu-opened', entrypointAndMenu);
          entrypointAndMenu.tabSuggestionsLoading = true;
          entrypointAndMenu.menuOpenDelayMs = 10;
          await microtasksFinished();

          const button = $$(entrypointAndMenu, '#entrypointButton')!;
          button.dispatchEvent(new Event('context-menu-entrypoint-click'));
          await microtasksFinished();

          assertFalse(entrypointAndMenu.$.menu.open);

          await openedPromise;

          assertTrue(entrypointAndMenu.$.menu.open);
        });

    test('menu opens immediately if not loading', async () => {
      const openedPromise =
          eventToPromise('context-menu-opened', entrypointAndMenu);
      const button = $$(entrypointAndMenu, '#entrypointButton')!;
      button.dispatchEvent(new Event('context-menu-entrypoint-click'));
      await openedPromise;

      assertTrue(entrypointAndMenu.$.menu.open);
    });

    test('hover event is fired', async () => {
      const hoverEventPromise =
          eventToPromise('context-menu-entrypoint-hover', entrypointAndMenu);

      const entrypointButton = $$(entrypointAndMenu, '#entrypointButton');
      assertTrue(!!entrypointButton);

      const hoverTarget = entrypointButton.shadowRoot!.querySelector('div')!;
      assertTrue(!!hoverTarget);
      hoverTarget.dispatchEvent(new Event('pointerenter'));

      assertTrue(!!(await hoverEventPromise));
    });
  });
});

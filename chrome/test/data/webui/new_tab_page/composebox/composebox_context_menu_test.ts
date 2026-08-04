// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {TabUploadOrigin} from 'chrome://resources/cr_components/composebox/common.js';
import type {ComposeboxState} from 'chrome://resources/cr_components/composebox/common.js';
import {ContextUploadErrorType, InputType, ToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {ADD_TAB_CONTEXT_FN, createComposeboxElement, FAKE_TOKEN_STRING, MockInputState, setupComposeboxTest} from './test_support.js';

suite(`NewTabPageComposeboxContextMenuTest`, () => {
  const testProxy = setupComposeboxTest();

  suite('Context menu', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        composeboxShowRecentTabChip: true,
        composeboxShowContextMenu: true,
      });
    });

    test('context button visible', () => {
      createComposeboxElement(testProxy);

      const contextMenuButton = $$(testProxy.element, '#contextEntrypoint');
      assertTrue(!!contextMenuButton);
    });

    test(
        'composebox context menu disables cr-action-menu auto-reposition',
        async () => {
          createComposeboxElement(testProxy);

          const entrypointAndMenu = testProxy.element.shadowRoot.querySelector(
              'cr-composebox-contextual-entrypoint-and-menu');
          assertTrue(!!entrypointAndMenu);
          await entrypointAndMenu.updateComplete;
          assertTrue(entrypointAndMenu.disableAutoReposition);

          const contextualActionMenu =
              entrypointAndMenu.shadowRoot.querySelector(
                  'cr-composebox-contextual-action-menu');
          assertTrue(!!contextualActionMenu);
          await contextualActionMenu.updateComplete;

          const crActionMenu = contextualActionMenu.$.menu;
          assertTrue(!crActionMenu.autoReposition);
          assertTrue(!crActionMenu.hasAttribute('auto-reposition'));
        });

    test('add tab context', async () => {
      createComposeboxElement(testProxy);
      testProxy.searchboxHandler.setPromiseResolveFor(
          ADD_TAB_CONTEXT_FN, {low: BigInt(1), high: BigInt(2)});

      // Assert no files.
      assertFalse(!!$$<HTMLElement>(testProxy.element, '#carousel'));

      const contextMenuButton = $$(testProxy.element, '#contextEntrypoint');
      assertTrue(!!contextMenuButton);
      const sampleTabTitle = 'Sample Tab';
      contextMenuButton.dispatchEvent(new CustomEvent('add-tab-context', {
        detail: {id: 1, title: sampleTabTitle},
        bubbles: true,
        composed: true,
      }));

      await testProxy.searchboxHandler.whenCalled(ADD_TAB_CONTEXT_FN);
      await microtasksFinished();
      const files = testProxy.element.$.carousel.files;
      assertEquals(files.length, 1);
      assertEquals(files[0]!.type, 'tab');
      assertEquals(files[0]!.name, sampleTabTitle);
    });

    test('add tab context fails', async () => {
      createComposeboxElement(testProxy);
      // Set the promise to reject to simulate a failure.
      testProxy.searchboxHandler.setResultMapperFor(ADD_TAB_CONTEXT_FN, () => {
        return Promise.reject(ContextUploadErrorType.kBrowserProcessingError);
      });

      // Assert no files.
      assertFalse(!!$$<HTMLElement>(testProxy.element, '#carousel'));

      const contextMenuButton = $$(testProxy.element, '#contextEntrypoint');
      assertTrue(!!contextMenuButton);
      const sampleTabTitle = 'Sample Tab';
      let contextAdded = false;
      const callback = (_file: unknown) => {
        contextAdded = true;
      };

      contextMenuButton.dispatchEvent(new CustomEvent('add-tab-context', {
        detail: {id: 1, title: sampleTabTitle, onContextAdded: callback},
        bubbles: true,
        composed: true,
      }));

      await testProxy.searchboxHandler.whenCalled(ADD_TAB_CONTEXT_FN);
      await microtasksFinished();

      // Assert callback was not called and no files in carousel.
      assertFalse(contextAdded);
      assertFalse(!!$$<HTMLElement>(testProxy.element, '#carousel'));

      assertEquals(
          loadTimeData.getString('composeboxFileUploadFailed'),
          testProxy.element.$.errorScrim.errorMessage);
    });

    test('tab changes calls getRecentTabs', async () => {
      loadTimeData.overrideValues({
        composeboxShowRecentTabChip: true,
      });
      const sampleTabs = [
        {
          tabId: 1,
          title: 'Sample Tab 1',
          url: 'https://example.com/1',
          showInRecentTabChip: true,
          lastActive: {internalValue: BigInt(1)},
        },
        {
          tabId: 2,
          title: 'Sample Tab 2',
          url: 'https://example.com/2',
          showInRecentTabChip: true,
          lastActive: {internalValue: BigInt(2)},
        },
      ];

      testProxy.searchboxHandler.setResultFor(
          'getRecentTabs', Promise.resolve({tabs: sampleTabs}));
      createComposeboxElement(testProxy);

      const entrypointAndMenu = testProxy.element.shadowRoot.querySelector(
          'cr-composebox-contextual-entrypoint-and-menu');
      assertTrue(!!entrypointAndMenu, 'contextual-entrypoint-and-menu');
      const contextMenuEntrypoint = entrypointAndMenu.shadowRoot.querySelector(
          'cr-composebox-contextual-entrypoint-button');
      assertTrue(!!contextMenuEntrypoint, 'contextual entrypoint button');
      const entrypointButton =
          contextMenuEntrypoint.shadowRoot.querySelector<HTMLElement>(
              '#entrypoint');
      assertTrue(!!entrypointButton, 'Entrypoint button');
      entrypointButton.click();

      // Await mojo to finish.
      await testProxy.searchboxHandler.whenCalled('getRecentTabs');
      // Await for .then() promise to run.
      await microtasksFinished();

      const initialCallCount =
          testProxy.searchboxHandler.getCallCount('getRecentTabs');

      // Assert another call to `getRecentTabs` is made on tab changes.
      testProxy.searchboxCallbackRouterRemote.onTabStripChanged();
      await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
      assertEquals(
          testProxy.searchboxHandler.getCallCount('getRecentTabs'),
          initialCallCount + 1);
      assertEquals(testProxy.element.recentTabId, 1);
      assertEquals(entrypointAndMenu.recentTabId, 1);
    });

    test(
        'recentTabId is passed to' +
            ' cr-composebox-contextual-entrypoint-and-menu' +
            ' and cr-composebox-contextual-action-menu',
        async () => {
          loadTimeData.overrideValues({
            composeboxShowRecentTabChip: true,
          });
          const sampleTabs = [
            {
              tabId: 42,
              title: 'Sample Tab 42',
              url: 'about:blank/42',
              showInRecentTabChip: true,
              lastActive: {internalValue: BigInt(1)},
            },
            {
              tabId: 43,
              title: 'Sample Tab 43',
              url: 'about:blank/43',
              showInRecentTabChip: true,
              lastActive: {internalValue: BigInt(2)},
            },
          ];

          testProxy.searchboxHandler.setResultFor(
              'getRecentTabs', Promise.resolve({tabs: sampleTabs}));
          createComposeboxElement(testProxy);

          const entrypointAndMenu = testProxy.element.shadowRoot.querySelector(
              'cr-composebox-contextual-entrypoint-and-menu');
          assertTrue(!!entrypointAndMenu);

          const contextMenuEntrypoint =
              entrypointAndMenu.shadowRoot.querySelector(
                  'cr-composebox-contextual-entrypoint-button');
          assertTrue(!!contextMenuEntrypoint);
          const entrypointButton =
              contextMenuEntrypoint.shadowRoot.querySelector<HTMLElement>(
                  '#entrypoint');
          assertTrue(!!entrypointButton);
          entrypointButton.click();

          // Await mojo to finish.
          await testProxy.searchboxHandler.whenCalled('getRecentTabs');
          // Await for .then() promise to run.
          await microtasksFinished();
          // Await for composebox to pass the `recentTabID` down.
          await testProxy.element.updateComplete;
          // Await for the `entrypointAndMenu` to pass the `recentTabID`
          // down.
          await entrypointAndMenu.updateComplete;

          // Assert recent tab is set across layers.
          assertEquals(testProxy.element.recentTabId, 42);
          assertEquals(entrypointAndMenu.recentTabId, 42);

          const contextualActionMenu =
              entrypointAndMenu.shadowRoot.querySelector(
                  'cr-composebox-contextual-action-menu');
          assertTrue(!!contextualActionMenu);

          await contextualActionMenu.updateComplete;
          // Assert recent tab is set across layers.
          assertEquals(contextualActionMenu.recentTabId, 42);
        });

    test(
        'context menu opens below anchor when space is sufficient',
        async () => {
          createComposeboxElement(testProxy);

          // Get the contextual action menu element.
          const entrypointAndMenu = testProxy.element.shadowRoot.querySelector(
              'cr-composebox-contextual-entrypoint-and-menu');
          assertTrue(!!entrypointAndMenu);
          const contextActionMenu = entrypointAndMenu.shadowRoot.querySelector(
              'cr-composebox-contextual-action-menu');
          assertTrue(!!contextActionMenu);

          // Create a fake anchor near the viewport top to guarantee
          // sufficient space below.
          const fakeAnchor = document.createElement('div');
          fakeAnchor.style.cssText = `
          position: fixed;
          top: 10px;
          left: 100px;
          height: 36px;
          width: 40px;
        `;
          document.body.appendChild(fakeAnchor);

          // Open the menu at the fake anchor.
          contextActionMenu.showAt(fakeAnchor);
          await microtasksFinished();

          try {
            // Access the dialog.
            const crActionMenu =
                contextActionMenu.shadowRoot.querySelector('#menu');
            assertTrue(!!crActionMenu);
            const dialog = crActionMenu.shadowRoot!.querySelector('dialog');
            assertTrue(!!dialog);
            assertTrue(dialog.open);

            // Assert: menu should open below the anchor.
            const anchorRect = fakeAnchor.getBoundingClientRect();
            const dialogRect = dialog.getBoundingClientRect();
            assertTrue(dialogRect.height > 0);
            assertTrue(dialogRect.top >= anchorRect.bottom - 1);
          } finally {
            // Clean even if the assertions fail.
            contextActionMenu.close();
            fakeAnchor.remove();
          }
        });

    test('context menu flips above when insufficient space below', async () => {
      createComposeboxElement(testProxy);

      // Push an input state with tools so the menu has visible
      // content.
      const inputState = new MockInputState({
        allowedTools: [ToolMode.kDeepSearch],
      });
      testProxy.searchboxCallbackRouterRemote.onInputStateChanged(inputState);
      await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
      await microtasksFinished();

      // Get the contextual action menu element.
      const entrypointAndMenu = testProxy.element.shadowRoot.querySelector(
          'cr-composebox-contextual-entrypoint-and-menu');
      assertTrue(!!entrypointAndMenu);
      const contextActionMenu = entrypointAndMenu.shadowRoot.querySelector(
          'cr-composebox-contextual-action-menu');
      assertTrue(!!contextActionMenu);

      // Create a fake anchor near the viewport bottom to force a
      // flip.
      const fakeAnchor = document.createElement('div');
      fakeAnchor.style.cssText = `
          position: fixed;
          bottom: 10px;
          left: 100px;
          height: 36px;
          width: 40px;
        `;
      document.body.appendChild(fakeAnchor);

      // Open the menu anchored to the fake element.
      contextActionMenu.showAt(fakeAnchor);
      await microtasksFinished();

      try {
        // Access the dialog.
        const crActionMenu =
            contextActionMenu.shadowRoot.querySelector('#menu');
        assertTrue(!!crActionMenu);
        const dialog = crActionMenu.shadowRoot!.querySelector('dialog');
        assertTrue(!!dialog);
        assertTrue(dialog.open);

        // Assert: menu should flip above the anchor.
        const anchorRect = fakeAnchor.getBoundingClientRect();
        const dialogRect = dialog.getBoundingClientRect();
        assertTrue(dialogRect.height > anchorRect.height);
        assertTrue(dialogRect.top < anchorRect.top);
      } finally {
        // Clean even if the assertions fail.
        contextActionMenu.close();
        fakeAnchor.remove();
      }
    });

    test('selected tabs are displayed at the top of the list', async () => {
      const sampleTabs = [
        {
          tabId: 1,
          title: 'Tab 1',
          url: 'https://example.com/1',
          showInRecentTabChip: true,
          lastActive: {internalValue: BigInt(1)},
        },
        {
          tabId: 2,
          title: 'Tab 2',
          url: 'https://example.com/2',
          showInRecentTabChip: true,
          lastActive: {internalValue: BigInt(2)},
        },
        {
          tabId: 3,
          title: 'Tab 3',
          url: 'https://example.com/3',
          showInRecentTabChip: true,
          lastActive: {internalValue: BigInt(3)},
        },
      ];

      testProxy.searchboxHandler.setResultFor(
          'getRecentTabs', Promise.resolve({tabs: sampleTabs}));

      createComposeboxElement(testProxy);

      // Select tabId 2 by setting it in addedTabsIds.
      testProxy.element.addedTabsIds = new Map([[2, '1']]);

      // Click entrypoint button to show the menu and load/sort
      // suggestions.
      const entrypointAndMenu = testProxy.element.shadowRoot.querySelector(
          'cr-composebox-contextual-entrypoint-and-menu');
      assertTrue(!!entrypointAndMenu);
      const contextMenuEntrypoint = entrypointAndMenu.shadowRoot.querySelector(
          'cr-composebox-contextual-entrypoint-button');
      assertTrue(!!contextMenuEntrypoint);
      const entrypointButton =
          contextMenuEntrypoint.shadowRoot.querySelector<HTMLElement>(
              '#entrypoint');
      assertTrue(!!entrypointButton);
      entrypointButton.click();
      await microtasksFinished();
      await testProxy.element.updateComplete;

      // Assert that tabSuggestions passed down to entrypointAndMenu
      // has the selected tab at the top.
      const sortedSuggestions = entrypointAndMenu.tabSuggestions;
      assertEquals(sortedSuggestions.length, 3);
      assertEquals(sortedSuggestions[0]!.tabId, 2);
      assertEquals(sortedSuggestions[1]!.tabId, 1);
      assertEquals(sortedSuggestions[2]!.tabId, 3);
    });

    test('clicking sorted suggestions adds the correct tab', async () => {
      const sampleTabs = [
        {
          tabId: 1,
          title: 'Tab 1',
          url: 'https://example.com/1',
          showInRecentTabChip: true,
          lastActive: {internalValue: BigInt(1)},
        },
        {
          tabId: 2,
          title: 'Tab 2',
          url: 'https://example.com/2',
          showInRecentTabChip: true,
          lastActive: {internalValue: BigInt(2)},
        },
      ];

      testProxy.searchboxHandler.setResultFor(
          'getRecentTabs', Promise.resolve({tabs: sampleTabs}));

      createComposeboxElement(testProxy);
      const inputState = new MockInputState({
        allowedInputTypes: [InputType.kBrowserTab],
      });
      testProxy.searchboxCallbackRouterRemote.onInputStateChanged(inputState);
      await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();

      // Click entrypoint button to show the menu and load suggestions.
      const entrypointAndMenu = testProxy.element.shadowRoot.querySelector(
          'cr-composebox-contextual-entrypoint-and-menu');
      assertTrue(!!entrypointAndMenu);
      const contextMenuEntrypoint = entrypointAndMenu.shadowRoot.querySelector(
          'cr-composebox-contextual-entrypoint-button');
      assertTrue(!!contextMenuEntrypoint);
      const entrypointButton =
          contextMenuEntrypoint.shadowRoot.querySelector<HTMLElement>(
              '#entrypoint');
      assertTrue(!!entrypointButton);
      entrypointButton.click();
      await microtasksFinished();

      assertEquals(testProxy.element.tabSuggestions.length, 2);

      testProxy.element.addedTabsIds = new Map([[2, '1']]);
      await testProxy.element.updateComplete;
      await entrypointAndMenu.updateComplete;

      const contextualActionMenu = entrypointAndMenu.shadowRoot.querySelector(
          'cr-composebox-contextual-action-menu');
      assertTrue(!!contextualActionMenu);
      await contextualActionMenu.updateComplete;
      await microtasksFinished();

      const items =
          contextualActionMenu.$.menu.querySelectorAll<HTMLButtonElement>(
              '.dropdown-item[data-index]');
      assertEquals(items.length, 2);

      // Find the button for Tab 1 (which was index 0 originally, but now
      // index 1 after sorting).
      const tab1Button = [...items].find(
          item => item.getAttribute('title') === 'Tab 1');
      assertTrue(!!tab1Button);

      // Reset mock and set promise resolver.
      testProxy.searchboxHandler.reset();
      testProxy.searchboxHandler.setPromiseResolveFor(
          ADD_TAB_CONTEXT_FN, {low: BigInt(3), high: BigInt(4)});

      // Click Tab 1 button.
      tab1Button.click();

      // Verify that addTabContext is called with tabId 1, not 2.
      const args =
          await testProxy.searchboxHandler.whenCalled(ADD_TAB_CONTEXT_FN);
      assertEquals(args[0], 1);
    });

    test('keeps menu open during slow tab add', async () => {
      loadTimeData.overrideValues({
        contextManagementInComposeboxEnabled: true,
        keepMenuOpenOnTabSelectForRealbox: true,
        composeboxContextMenuEnableMultiTabSelection: true,
      });
      const sampleTabs = [{
        tabId: 1,
        title: 'Tab 1',
        url: 'https://example.com/1',
        showInRecentTabChip: true,
        lastActive: {internalValue: BigInt(1)},
      }];
      testProxy.searchboxHandler.setResultFor(
          'getRecentTabs', Promise.resolve({tabs: sampleTabs}));

      createComposeboxElement(testProxy);

      const inputState = new MockInputState({
        allowedInputTypes: [InputType.kBrowserTab],
      });
      testProxy.searchboxCallbackRouterRemote.onInputStateChanged(inputState);
      await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();

      const entrypointAndMenu = testProxy.element.shadowRoot.querySelector(
          'cr-composebox-contextual-entrypoint-and-menu');
      assertTrue(!!entrypointAndMenu);
      const contextMenuEntrypoint = entrypointAndMenu.shadowRoot.querySelector(
          'cr-composebox-contextual-entrypoint-button');
      assertTrue(!!contextMenuEntrypoint);
      const entrypointButton =
          contextMenuEntrypoint.shadowRoot.querySelector<HTMLElement>(
              '#entrypoint');
      assertTrue(!!entrypointButton);
      entrypointButton.click();
      await microtasksFinished();

      const contextualActionMenu = entrypointAndMenu.shadowRoot.querySelector(
          'cr-composebox-contextual-action-menu');
      assertTrue(!!contextualActionMenu);
      await contextualActionMenu.updateComplete;

      const trigger =
          contextualActionMenu.shadowRoot.querySelector('#shareTabsTrigger');
      assertTrue(!!trigger);
      trigger.dispatchEvent(new PointerEvent('pointerenter'));
      await contextualActionMenu.updateComplete;
      assertTrue(contextualActionMenu.shareTabsFlyoutOpen);

      // Simulate moving pointer from trigger to flyout.
      trigger.dispatchEvent(new PointerEvent('pointerleave'));
      const flyout =
          contextualActionMenu.shadowRoot.querySelector('.share-tabs-flyout');
      assertTrue(!!flyout);
      flyout.dispatchEvent(new PointerEvent('pointerenter'));
      await contextualActionMenu.updateComplete;

      let resolveAddTab: (value: UnguessableToken|null) => void = () => {};
      const addTabPromise = new Promise<UnguessableToken|null>(resolve => {
        resolveAddTab = resolve;
      });
      testProxy.searchboxHandler.setResultFor(
          ADD_TAB_CONTEXT_FN, addTabPromise);

      const item =
          contextualActionMenu.shadowRoot.querySelector<HTMLButtonElement>(
              '.share-tabs-flyout .dropdown-item[data-index="0"]');
      assertTrue(!!item);
      item.click();

      await testProxy.searchboxHandler.whenCalled(ADD_TAB_CONTEXT_FN);

      // Simulate leaving the flyout while tab is being added.
      flyout.dispatchEvent(new PointerEvent('pointerleave'));

      // Wait 1200ms (FIRST_TAB_DELAY is 1000ms).
      await new Promise(resolve => setTimeout(resolve, 1200));

      // Should still be open because firstTabBeingAdded_ is true (5000ms
      // safety timer).
      assertTrue(contextualActionMenu.shareTabsFlyoutOpen);

      // Resolve the add tab call.
      resolveAddTab(FAKE_TOKEN_STRING);
      await microtasksFinished();

      // Simulate the mixin updating addedTabsIds.
      testProxy.element.addedTabsIds = new Map([[1, FAKE_TOKEN_STRING]]);
      await testProxy.element.updateComplete;
      await entrypointAndMenu.updateComplete;
      await contextualActionMenu.updateComplete;

      // Wait for both firstTabBeingAdded timer (1000ms) and close timer
      // (300ms) + buffer.
      await new Promise(resolve => setTimeout(resolve, 1500));

      // Should now be closed automatically because pointer left and tab
      // was added.
      assertFalse(contextualActionMenu.shareTabsFlyoutOpen);
    });

    test(
        'reopens menu when initialized with tab from context menu',
        async () => {
          loadTimeData.overrideValues({
            contextManagementInComposeboxEnabled: true,
            keepMenuOpenOnTabSelectForRealbox: true,
            composeboxContextMenuEnableMultiTabSelection: true,
          });

          const sampleTabs = [{
            tabId: 1,
            title: 'Tab 1',
            url: 'https://example.com/1',
            showInRecentTabChip: true,
            lastActive: {internalValue: BigInt(1)},
          }];
          testProxy.searchboxHandler.setResultFor(
              'getRecentTabs', Promise.resolve({tabs: sampleTabs}));

          const initialState: ComposeboxState = {
            text: '',
            files: [{
              tabId: 1,
              url: 'https://example.com/1',
              title: 'Tab 1',
              origin: TabUploadOrigin.CONTEXT_MENU,
              delayUpload: false,
            }],
            mode: ToolMode.kUnspecified,
            model: 0,
            smartTabSharingActive: false,
          };

          createComposeboxElement(testProxy, {state: initialState});
          await testProxy.element.updateComplete;

          const entrypointAndMenu = testProxy.element.shadowRoot.querySelector(
              'cr-composebox-contextual-entrypoint-and-menu');
          assertTrue(!!entrypointAndMenu);
          await entrypointAndMenu.updateComplete;

          const contextualActionMenu =
              entrypointAndMenu.shadowRoot.querySelector(
                  'cr-composebox-contextual-action-menu');
          assertTrue(!!contextualActionMenu);
          await contextualActionMenu.updateComplete;

          // Verify that the menu is open and the flyout is open.
          assertTrue(contextualActionMenu.shareTabsFlyoutOpen);
        });

    test(
        'context menu is rendered when contextMenuEnabled is true, ' +
            'regardless of tabs',
        async () => {
          createComposeboxElement(testProxy);

          // Verify with tabs
          testProxy.element.hasTabs = () => true;
          testProxy.element.contextMenuEnabled = true;
          await microtasksFinished();
          await testProxy.element.updateComplete;

          let contextMenus = testProxy.element.shadowRoot.querySelectorAll(
              '#contextMenuContainer');
          assertEquals(1, contextMenus.length);  // Not 2 of them; only 1.

          testProxy.element.contextMenuEnabled = false;
          await microtasksFinished();
          await testProxy.element.updateComplete;

          contextMenus = testProxy.element.shadowRoot.querySelectorAll(
              '#contextMenuContainer');
          assertEquals(0, contextMenus.length);

          // Verify without tabs
          testProxy.element.hasTabs = () => false;
          testProxy.element.contextMenuEnabled = true;
          await microtasksFinished();
          await testProxy.element.updateComplete;

          // When there are no tabs, the context menu is now unconditionally
          // rendered if contextMenuEnabled is true.
          contextMenus = testProxy.element.shadowRoot.querySelectorAll(
              '#contextMenuContainer');
          assertEquals(1, contextMenus.length);

          testProxy.element.contextMenuEnabled = false;
          await microtasksFinished();
          await testProxy.element.updateComplete;

          contextMenus = testProxy.element.shadowRoot.querySelectorAll(
              '#contextMenuContainer');
          assertEquals(0, contextMenus.length);
        });

    test(
        'smart tab sharing item is disabled when tab input type is disabled',
        async () => {
          loadTimeData.overrideValues({
            contextManagementInComposeboxEnabled: true,
            composeboxSmartTabSharingVisible: true,
          });
          testProxy.searchboxHandler.setPromiseResolveFor(
              'getSmartTabSharingActive', {active: true});
          createComposeboxElement(testProxy);
          await testProxy.element.updateComplete;

          // Initially enabled (not disabled)
          const inputStateEnabled = new MockInputState({
            allowedInputTypes: [InputType.kBrowserTab],
            disabledInputTypes: [],
          });
          testProxy.searchboxCallbackRouterRemote.onInputStateChanged(
              inputStateEnabled);
          await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
          await testProxy.element.updateComplete;

          // Open menu
          const entrypointAndMenu = testProxy.element.shadowRoot.querySelector(
              'cr-composebox-contextual-entrypoint-and-menu');
          assertTrue(!!entrypointAndMenu);
          const contextMenuEntrypoint =
              entrypointAndMenu.shadowRoot.querySelector(
                  'cr-composebox-contextual-entrypoint-button');
          assertTrue(!!contextMenuEntrypoint);
          const entrypointButton =
              contextMenuEntrypoint.shadowRoot.querySelector<HTMLElement>(
                  '#entrypoint');
          assertTrue(!!entrypointButton);
          entrypointButton.click();
          await microtasksFinished();
          await entrypointAndMenu.updateComplete;

          const contextualActionMenu =
              entrypointAndMenu.shadowRoot.querySelector(
                  'cr-composebox-contextual-action-menu');
          assertTrue(!!contextualActionMenu);
          await contextualActionMenu.updateComplete;

          const smartTabSharingItem =
              contextualActionMenu.shadowRoot.querySelector<HTMLButtonElement>(
                  '#smartTabSharingItem');
          assertTrue(!!smartTabSharingItem);
          assertFalse(smartTabSharingItem.disabled);

          // Close menu
          contextualActionMenu.close();
          await microtasksFinished();

          // Disable tab input
          const inputStateDisabled = new MockInputState({
            allowedInputTypes: [InputType.kBrowserTab],
            disabledInputTypes: [InputType.kBrowserTab],
          });
          testProxy.searchboxCallbackRouterRemote.onInputStateChanged(
              inputStateDisabled);
          await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
          await testProxy.element.updateComplete;

          // Open menu again
          entrypointButton.click();
          await microtasksFinished();
          await entrypointAndMenu.updateComplete;
          await contextualActionMenu.updateComplete;

          const smartTabSharingItemDisabled =
              contextualActionMenu.shadowRoot.querySelector<HTMLButtonElement>(
                  '#smartTabSharingItem');
          assertTrue(!!smartTabSharingItemDisabled);
          assertTrue(smartTabSharingItemDisabled.disabled);
        });

    test(
        'share tabs trigger is disabled when tab input type is disabled',
        async () => {
          loadTimeData.overrideValues({
            contextManagementInComposeboxEnabled: true,
            composeboxSmartTabSharingVisible: true,
          });
          testProxy.searchboxHandler.setPromiseResolveFor(
              'getSmartTabSharingActive', {active: false});

          const sampleTabs = [{
            tabId: 1,
            title: 'Tab 1',
            url: 'https://example.com',
            showInCurrentTabChip: true,
            showInPreviousTabChip: false,
            lastActive: {internalValue: BigInt(1)},
          }];
          testProxy.searchboxHandler.setResultFor(
              'getRecentTabs', Promise.resolve({tabs: sampleTabs}));

          createComposeboxElement(testProxy);
          await testProxy.element.updateComplete;

          const inputStateDisabled = new MockInputState({
            allowedInputTypes: [InputType.kBrowserTab],
            disabledInputTypes: [InputType.kBrowserTab],
          });
          testProxy.searchboxCallbackRouterRemote.onInputStateChanged(
              inputStateDisabled);
          await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
          await testProxy.element.updateComplete;

          // Open menu
          const entrypointAndMenu = testProxy.element.shadowRoot.querySelector(
              'cr-composebox-contextual-entrypoint-and-menu');
          assertTrue(!!entrypointAndMenu, 'entrypointAndMenu should exist');
          const contextMenuEntrypoint =
              entrypointAndMenu.shadowRoot.querySelector(
                  'cr-composebox-contextual-entrypoint-button');
          assertTrue(
              !!contextMenuEntrypoint, 'contextMenuEntrypoint should exist');
          const entrypointButton =
              contextMenuEntrypoint.shadowRoot.querySelector<HTMLElement>(
                  '#entrypoint');
          assertTrue(!!entrypointButton, 'entrypointButton should exist');
          entrypointButton.click();
          await microtasksFinished();
          await entrypointAndMenu.updateComplete;

          const contextualActionMenu =
              entrypointAndMenu.shadowRoot.querySelector(
                  'cr-composebox-contextual-action-menu');
          assertTrue(
              !!contextualActionMenu, 'contextualActionMenu should exist');
          await contextualActionMenu.updateComplete;

          const shareTabsTrigger =
              contextualActionMenu.shadowRoot.querySelector<HTMLButtonElement>(
                  '#shareTabsTrigger');
          assertTrue(!!shareTabsTrigger, 'shareTabsTrigger should exist');
          assertTrue(
              shareTabsTrigger.disabled, 'shareTabsTrigger should be disabled');
        });

    test(
        'hovering on disabled share tabs trigger does not open flyout',
        async () => {
          loadTimeData.overrideValues({
            contextManagementInComposeboxEnabled: true,
            composeboxSmartTabSharingVisible: true,
          });
          testProxy.searchboxHandler.setPromiseResolveFor(
              'getSmartTabSharingActive', {active: false});

          const sampleTabs = [{
            tabId: 1,
            title: 'Tab 1',
            url: 'https://example.com',
            showInCurrentTabChip: true,
            showInPreviousTabChip: false,
            lastActive: {internalValue: BigInt(1)},
          }];
          testProxy.searchboxHandler.setResultFor(
              'getRecentTabs', Promise.resolve({tabs: sampleTabs}));

          createComposeboxElement(testProxy);
          await testProxy.element.updateComplete;

          const inputStateDisabled = new MockInputState({
            allowedInputTypes: [InputType.kBrowserTab],
            disabledInputTypes: [InputType.kBrowserTab],
          });
          testProxy.searchboxCallbackRouterRemote.onInputStateChanged(
              inputStateDisabled);
          await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
          await testProxy.element.updateComplete;

          // Open menu
          const entrypointAndMenu = testProxy.element.shadowRoot.querySelector(
              'cr-composebox-contextual-entrypoint-and-menu');
          assertTrue(!!entrypointAndMenu);
          const contextMenuEntrypoint =
              entrypointAndMenu.shadowRoot.querySelector(
                  'cr-composebox-contextual-entrypoint-button');
          assertTrue(!!contextMenuEntrypoint);
          const entrypointButton =
              contextMenuEntrypoint.shadowRoot.querySelector<HTMLElement>(
                  '#entrypoint');
          assertTrue(!!entrypointButton);
          entrypointButton.click();
          await microtasksFinished();
          await entrypointAndMenu.updateComplete;

          const contextualActionMenu =
              entrypointAndMenu.shadowRoot.querySelector(
                  'cr-composebox-contextual-action-menu');
          assertTrue(!!contextualActionMenu);
          await contextualActionMenu.updateComplete;

          const shareTabsTrigger =
              contextualActionMenu.shadowRoot.querySelector<HTMLButtonElement>(
                  '#shareTabsTrigger');
          assertTrue(!!shareTabsTrigger);
          assertTrue(shareTabsTrigger.disabled);

          // Simulate pointerenter (hover) on the disabled trigger.
          shareTabsTrigger.dispatchEvent(new PointerEvent('pointerenter'));
          await microtasksFinished();
          await contextualActionMenu.updateComplete;

          // Verify flyout is NOT open.
          assertFalse(
              contextualActionMenu.shareTabsFlyoutOpen,
              'flyout open property should be false');
          const flyout =
              contextualActionMenu.shadowRoot.querySelector<HTMLElement>(
                  '.share-tabs-flyout');
          assertTrue(!!flyout, 'flyout element should exist');
          assertTrue(
              flyout.hidden, 'flyout element should have hidden attribute');
          assertEquals(
              'none', window.getComputedStyle(flyout).display,
              'flyout element should be display:none');
        });

    test(
        'smart tab sharing item in flyout is disabled when tab input type is disabled',
        async () => {
          loadTimeData.overrideValues({
            contextManagementInComposeboxEnabled: true,
            composeboxSmartTabSharingVisible: true,
          });
          testProxy.searchboxHandler.setPromiseResolveFor(
              'getSmartTabSharingActive', {active: false});

          const sampleTabs = [{
            tabId: 1,
            title: 'Tab 1',
            url: 'https://example.com',
            showInCurrentTabChip: true,
            showInPreviousTabChip: false,
            lastActive: {internalValue: BigInt(1)},
          }];
          testProxy.searchboxHandler.setResultFor(
              'getRecentTabs', Promise.resolve({tabs: sampleTabs}));

          createComposeboxElement(testProxy);
          await testProxy.element.updateComplete;

          const inputStateDisabled = new MockInputState({
            allowedInputTypes: [InputType.kBrowserTab],
            disabledInputTypes: [InputType.kBrowserTab],
          });
          testProxy.searchboxCallbackRouterRemote.onInputStateChanged(
              inputStateDisabled);
          await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
          await testProxy.element.updateComplete;

          // Open menu
          const entrypointAndMenu = testProxy.element.shadowRoot.querySelector(
              'cr-composebox-contextual-entrypoint-and-menu');
          assertTrue(!!entrypointAndMenu, 'entrypointAndMenu should exist');
          const contextMenuEntrypoint =
              entrypointAndMenu.shadowRoot.querySelector(
                  'cr-composebox-contextual-entrypoint-button');
          assertTrue(
              !!contextMenuEntrypoint, 'contextMenuEntrypoint should exist');
          const entrypointButton =
              contextMenuEntrypoint.shadowRoot.querySelector<HTMLElement>(
                  '#entrypoint');
          assertTrue(!!entrypointButton, 'entrypointButton should exist');
          entrypointButton.click();
          await microtasksFinished();
          await entrypointAndMenu.updateComplete;

          const contextualActionMenu =
              entrypointAndMenu.shadowRoot.querySelector(
                  'cr-composebox-contextual-action-menu');
          assertTrue(
              !!contextualActionMenu, 'contextualActionMenu should exist');
          await contextualActionMenu.updateComplete;

          // Force flyout open
          testProxy.element.shareTabsFlyoutOpen = true;
          await testProxy.element.updateComplete;
          await entrypointAndMenu.updateComplete;
          await contextualActionMenu.updateComplete;

          const smartTabSharingItemFlyout =
              contextualActionMenu.shadowRoot.querySelector<HTMLButtonElement>(
                  '#smartTabSharingItemFlyout');
          assertTrue(
              !!smartTabSharingItemFlyout,
              'smartTabSharingItemFlyout should exist');
          assertTrue(
              smartTabSharingItemFlyout.disabled,
              'smartTabSharingItemFlyout should be disabled');
        });
  });

  suite('Context menu mouse events', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        composeboxShowContextMenu: true,
      });
    });

    test(
        'mousedown does not prevent default when layout is Compact',
        async () => {
          createComposeboxElement(testProxy);
          testProxy.element.searchboxLayoutMode = 'Compact';
          await testProxy.element.updateComplete;

          const contextMenuContainer =
              testProxy.element.shadowRoot.querySelector(
                  '#contextMenuContainer');
          assertTrue(!!contextMenuContainer);

          const mousedownEvent = new MouseEvent('mousedown', {
            bubbles: true,
            cancelable: true,
            composed: false,
          });

          contextMenuContainer.dispatchEvent(mousedownEvent);
          assertFalse(mousedownEvent.defaultPrevented);
        });

    test('mouse click does not focus input when Compact', async () => {
      createComposeboxElement(testProxy);
      testProxy.element.searchboxLayoutMode = 'Compact';
      await testProxy.element.updateComplete;

      const contextMenuContainer =
          testProxy.element.shadowRoot.querySelector('#contextMenuContainer');
      assertTrue(!!contextMenuContainer);

      const clickEvent = new MouseEvent('click', {
        bubbles: true,
        cancelable: true,
        composed: false,
        button: 0,
      });

      let focusCalled = false;
      testProxy.element.focusInput = () => {
        focusCalled = true;
      };

      contextMenuContainer.dispatchEvent(clickEvent);
      assertTrue(clickEvent.defaultPrevented);
      assertFalse(focusCalled);
    });
  });
});

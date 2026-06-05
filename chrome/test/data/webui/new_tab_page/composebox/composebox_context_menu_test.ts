// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {ContextUploadErrorType, InputType, ToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {ADD_TAB_CONTEXT_FN, createComposeboxElement, MockInputState, setupComposeboxTest} from './test_support.js';

// ==========================================================
// 1. BASE SUITE (Runs ONLY on cr-composebox element)
// ==========================================================
suite('NewTabPageComposeboxContextMenuTest', () => {
  const testProxy = setupComposeboxTest();

  setup(() => {
    loadTimeData.overrideValues({
      useNtpComposeboxFork: false,
    });
  });

  suite('Context menu', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        composeboxShowRecentTabChip: true,
        composeboxShowContextMenu: true,
      });
    });

    test(
        'context menu button is displayed when not using pec api',
        async () => {
          loadTimeData.overrideValues({
            contextualMenuUsePecApi: false,
          });
          createComposeboxElement(testProxy);
          testProxy.element.showMenuOnClick = false;
          await testProxy.element.updateComplete;
          await microtasksFinished();

          const contextMenuButton = $$(testProxy.element, '#contextEntrypoint');

          assertTrue(!!contextMenuButton);
        });

    test(
        'context menu button is displayed with valid input state',
        async () => {
          loadTimeData.overrideValues({
            contextualMenuUsePecApi: true,
          });
          createComposeboxElement(testProxy);
          testProxy.element.showMenuOnClick = false;
          await testProxy.element.updateComplete;
          const inputState = new MockInputState({
            allowedTools: [ToolMode.kDeepSearch],
          });
          testProxy.searchboxCallbackRouterRemote.onInputStateChanged(
              inputState);
          await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
          await microtasksFinished();

          const contextMenuButton = $$(testProxy.element, '#contextEntrypoint');

          assertTrue(!!contextMenuButton);
        });

    test(
        'context menu button is hidden with invalid input state',
        async () => {
          loadTimeData.overrideValues({
            contextualMenuUsePecApi: true,
          });
          createComposeboxElement(testProxy);
          testProxy.element.showMenuOnClick = false;
          await microtasksFinished();

          const contextMenuButton = $$(testProxy.element, '#contextEntrypoint');

          assertFalse(!!contextMenuButton);
      });
  });

  suite('Context menu mouse events', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        composeboxShowContextMenu: true,
      });
    });

    test('mousedown prevents default when not Compact', async () => {
      createComposeboxElement(testProxy);
      testProxy.element.searchboxLayoutMode = 'TallBottomContext';
      await testProxy.element.updateComplete;

      const contextMenuContainer =
          testProxy.element.shadowRoot.querySelector('#contextMenuContainer');
      assertTrue(!!contextMenuContainer);

      const mousedownEvent = new MouseEvent('mousedown', {
        bubbles: true,
        cancelable: true,
        composed: false,
      });
      contextMenuContainer.dispatchEvent(mousedownEvent);
      assertTrue(mousedownEvent.defaultPrevented);
    });

    test(
        'mousedown does not prevent default when target is not container',
        async () => {
          createComposeboxElement(testProxy);
          testProxy.element.searchboxLayoutMode = 'TallBottomContext';
          await testProxy.element.updateComplete;

          const contextMenuContainer =
              testProxy.element.shadowRoot.querySelector(
                  '#contextMenuContainer');
          assertTrue(!!contextMenuContainer);

          const innerElement = document.createElement('div');
          innerElement.id = 'innerElement';
          contextMenuContainer.appendChild(innerElement);

          const mousedownEvent = new MouseEvent('mousedown', {
            bubbles: true,
            cancelable: true,
            composed: false,
          });

          innerElement.dispatchEvent(mousedownEvent);
          assertFalse(mousedownEvent.defaultPrevented);
        });

    test('mouse click prevents default and focuses input', async () => {
      createComposeboxElement(testProxy);
      testProxy.element.searchboxLayoutMode = 'TallBottomContext';
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
      assertTrue(focusCalled);
    });

    test('mouse click ignores non-primary button', async () => {
      createComposeboxElement(testProxy);
      testProxy.element.searchboxLayoutMode = 'TallBottomContext';
      await testProxy.element.updateComplete;

      const contextMenuContainer =
          testProxy.element.shadowRoot.querySelector('#contextMenuContainer');
      assertTrue(!!contextMenuContainer);

      const clickEvent = new MouseEvent('click', {
        bubbles: true,
        cancelable: true,
        composed: false,
        button: 1,
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

// =========================================================================
// 2. COMMON V2 SUITE (Runs on both ntp-composebox and cr-composebox elements)
// =========================================================================
[true, false].forEach(useForked => {
  suite(
      `NewTabPageComposeboxContextMenuTestV2 (useNtpComposeboxFork = ${
          useForked})`,
      () => {
        const testProxy = setupComposeboxTest();

        setup(() => {
          loadTimeData.overrideValues({
            useNtpComposeboxFork: useForked,
          });
        });

        suite('Context menu', () => {
          suiteSetup(() => {
            loadTimeData.overrideValues({
              composeboxShowRecentTabChip: true,
              composeboxShowContextMenu: true,
            });
          });

          test('context button visible', () => {
            createComposeboxElement(testProxy);

            const contextMenuButton =
                $$(testProxy.element, '#contextEntrypoint');
            assertTrue(!!contextMenuButton);
          });

          test('add tab context', async () => {
            createComposeboxElement(testProxy);
            testProxy.searchboxHandler.setPromiseResolveFor(
                ADD_TAB_CONTEXT_FN, {low: BigInt(1), high: BigInt(2)});

            // Assert no files.
            assertFalse(!!$$<HTMLElement>(testProxy.element, '#carousel'));

            const contextMenuButton =
                $$(testProxy.element, '#contextEntrypoint');
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
            testProxy.searchboxHandler.setResultMapperFor(
                ADD_TAB_CONTEXT_FN, () => {
                  return Promise.reject(
                      ContextUploadErrorType.kBrowserProcessingError);
                });

            // Assert no files.
            assertFalse(!!$$<HTMLElement>(testProxy.element, '#carousel'));

            const contextMenuButton =
                $$(testProxy.element, '#contextEntrypoint');
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
            createComposeboxElement(testProxy);
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
            const entrypointAndMenu =
                testProxy.element.shadowRoot.querySelector(
                    'cr-composebox-contextual-entrypoint-and-menu');
            assertTrue(!!entrypointAndMenu, 'contextual-entrypoint-and-menu');
            const contextMenuEntrypoint =
                entrypointAndMenu.shadowRoot.querySelector(
                    'cr-composebox-contextual-entrypoint-button');
            assertTrue(!!contextMenuEntrypoint, 'contextual entrypoint button');
            const entrypointButton =
                contextMenuEntrypoint.shadowRoot.querySelector<HTMLElement>(
                    '#entrypoint');
            assertTrue(!!entrypointButton, 'Entrypoint button');
            entrypointButton.click();
            await microtasksFinished();

            // There is an initial call to `getRecentTabs` on entrypoint click.
            assertEquals(
                testProxy.searchboxHandler.getCallCount('getRecentTabs'), 1);

            // Assert another call to `getRecentTabs` is made on tab changes.
            testProxy.searchboxCallbackRouterRemote.onTabStripChanged();
            await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
            assertEquals(
                testProxy.searchboxHandler.getCallCount('getRecentTabs'), 2);
          });

          test(
              'context menu opens below anchor when space is sufficient',
              async () => {
                createComposeboxElement(testProxy);

                // Get the contextual action menu element.
                const entrypointAndMenu =
                    testProxy.element.shadowRoot.querySelector(
                        'cr-composebox-contextual-entrypoint-and-menu');
                assertTrue(!!entrypointAndMenu);
                const contextActionMenu =
                    entrypointAndMenu.shadowRoot.querySelector(
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
                  const dialog =
                      crActionMenu.shadowRoot!.querySelector('dialog');
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

          test(
              'context menu flips above when insufficient space below',
              async () => {
                createComposeboxElement(testProxy);

                // Push an input state with tools so the menu has visible
                // content.
                const inputState = new MockInputState({
                  allowedTools: [ToolMode.kDeepSearch],
                });
                testProxy.searchboxCallbackRouterRemote.onInputStateChanged(
                    inputState);
                await testProxy.searchboxCallbackRouterRemote.$
                    .flushForTesting();
                await microtasksFinished();

                // Get the contextual action menu element.
                const entrypointAndMenu =
                    testProxy.element.shadowRoot.querySelector(
                        'cr-composebox-contextual-entrypoint-and-menu');
                assertTrue(!!entrypointAndMenu);
                const contextActionMenu =
                    entrypointAndMenu.shadowRoot.querySelector(
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
                  const dialog =
                      crActionMenu.shadowRoot!.querySelector('dialog');
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

          test(
              'selected tabs are displayed at the top of the list',
              async () => {
                createComposeboxElement(testProxy);
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

                // Select tabId 2 by setting it in addedTabsIds.
                testProxy.element.addedTabsIds = new Map([[2, '1']]);

                // Click entrypoint button to show the menu and load/sort
                // suggestions.
                const entrypointAndMenu =
                    testProxy.element.shadowRoot.querySelector(
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
            createComposeboxElement(testProxy);
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
            const inputState = new MockInputState({
              allowedInputTypes: [InputType.kBrowserTab],
            });
            testProxy.searchboxCallbackRouterRemote.onInputStateChanged(
                inputState);
            await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();

            // Click entrypoint button to show the menu and load suggestions.
            const entrypointAndMenu =
                testProxy.element.shadowRoot.querySelector(
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

            assertEquals(testProxy.element.tabSuggestions.length, 2);

            testProxy.element.addedTabsIds = new Map([[2, '1']]);
            await testProxy.element.updateComplete;
            await entrypointAndMenu.updateComplete;

            const contextualActionMenu =
                entrypointAndMenu.shadowRoot.querySelector(
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
            const tab1Button = Array.from(items).find(
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
                testProxy.element.shadowRoot.querySelector(
                    '#contextMenuContainer');
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
});

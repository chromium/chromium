// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {ContextUploadErrorType, ToolMode} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {ADD_TAB_CONTEXT_FN, createComposeboxElement, MockInputState, setupComposeboxTest} from './test_support.js';

suite('NewTabPageComposeboxContextMenuTest', () => {
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
      const callback = (_file: any) => {
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
        realboxLayoutMode: 'TallTopContext',
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
      await microtasksFinished();

      // There is an initial call to `getRecentTabs` on entrypoint click.
      assertEquals(testProxy.searchboxHandler.getCallCount('getRecentTabs'), 1);

      // Assert another call to `getRecentTabs` is made on tab changes.
      testProxy.searchboxCallbackRouterRemote.onTabStripChanged();
      await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
      assertEquals(testProxy.searchboxHandler.getCallCount('getRecentTabs'), 2);
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
      testProxy.element.searchboxLayoutMode = 'TallTopContext';
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

    test(
        'mousedown does not prevent default when target is not container',
        async () => {
          createComposeboxElement(testProxy);
          testProxy.element.searchboxLayoutMode = 'TallTopContext';
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
      testProxy.element.searchboxLayoutMode = 'TallTopContext';
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
      testProxy.element.searchboxLayoutMode = 'TallTopContext';
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

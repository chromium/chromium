// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {FileUploadErrorType, FileUploadStatus, InputType} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {ADD_FILE_CONTEXT_FN, ADD_TAB_CONTEXT_FN, createComposeboxElement, FAKE_TOKEN_STRING, FAKE_TOKEN_STRING_2, generateZeroId, mockInputState, setupComposeboxTest} from './test_support.js';

suite('NewTabPageComposeboxContextMenuTest', () => {
  const testProxy = setupComposeboxTest();

  async function addTab() {
    testProxy.searchboxHandler.setPromiseResolveFor(
        ADD_TAB_CONTEXT_FN, FAKE_TOKEN_STRING);

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
    return FAKE_TOKEN_STRING;
  }

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
        return Promise.reject(FileUploadErrorType.kBrowserProcessingError);
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

  test('autocomplete queried when autochip removed', async () => {
    createComposeboxElement(testProxy);
    await microtasksFinished();

    // Autocomplete queried once on load.
    assertEquals(
        testProxy.searchboxHandler.getCallCount('queryAutocomplete'), 1);
    testProxy.searchboxHandler.setPromiseResolveFor(
        ADD_TAB_CONTEXT_FN, {low: BigInt(1), high: BigInt(2)});

    const tab = {
      tabId: 1,
      title: 'Tab 1',
      url: 'https://example.com/1',
      showInCurrentTabChip: true,
      showInPreviousTabChip: false,
      lastActive: {internalValue: BigInt(1)},
    } as any as TabInfo;

    // Add autochip.
    testProxy.searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(tab);
    await microtasksFinished();

    // Should have cleared matches.
    assertEquals(
        1, testProxy.searchboxHandler.getCallCount('stopAutocomplete'));

    // Remove autochip.
    testProxy.searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(null);
    await microtasksFinished();

    // Autocomplete should be queried again when an auto chip is removed.
    assertEquals(
        3, testProxy.searchboxHandler.getCallCount('stopAutocomplete'));
    assertEquals(
        2, testProxy.searchboxHandler.getCallCount('queryAutocomplete'));
  });

  test(
      'autocomplete not requeried if file removed and autochip remains',
      async () => {
        const testInputState = {
          ...mockInputState,
          maxInstances: {
            [InputType.kBrowserTab]: 1,
            [InputType.kLensImage]: 3,
            [InputType.kLensFile]: 1,
          },
          maxTotalInputs: 3,
        };
        loadTimeData.overrideValues({composeboxShowZps: true});
        createComposeboxElement(testProxy);
        testProxy.searchboxCallbackRouterRemote.onInputStateChanged(
            testInputState);
        await microtasksFinished();

        // Autocomplete queried once on load.
        assertEquals(
            1, testProxy.searchboxHandler.getCallCount('queryAutocomplete'));

        const tab = {
          tabId: 1,
          title: 'Tab 1',
          url: 'https://example.com/1',
          showInCurrentTabChip: true,
          showInPreviousTabChip: false,
          lastActive: {internalValue: BigInt(1)},
        } as any as TabInfo;

        // Add autochip.
        const autochipToken = generateZeroId();
        testProxy.searchboxHandler.setPromiseResolveFor(
            ADD_TAB_CONTEXT_FN, {token: autochipToken});
        testProxy.searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
            tab);
        await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
        await testProxy.searchboxHandler.whenCalled(ADD_TAB_CONTEXT_FN);
        await microtasksFinished();

        // Autocomplete should NOT have been queried again when the chip was
        // added.
        assertEquals(
            1, testProxy.searchboxHandler.getCallCount('queryAutocomplete'));

        // Add a file.
        const fileId = generateZeroId();
        testProxy.searchboxHandler.setPromiseResolveFor(
            ADD_FILE_CONTEXT_FN, {token: fileId});

        testProxy.element.addFileContextForTesting({
          uuid: FAKE_TOKEN_STRING,
          name: 'foo.jpg',
          status: 0,
          type: 'image/jpeg',
          isDeletable: true,
          objectUrl: null,
          dataUrl: null,
          url: null,
          tabId: null,
          iconName: null,
        });
        await microtasksFinished();

        // Delete the uploaded file.
        const deletedId = testProxy.element.$.carousel.files[1]!.uuid;
        testProxy.element.$.carousel.dispatchEvent(
            new CustomEvent('delete-file', {
              detail: {
                uuid: deletedId,
              },
              bubbles: true,
              composed: true,
            }));

        await microtasksFinished();

        // Autocomplete should NOT be queried again when there is an autochip
        // remaining.
        assertEquals(
            1, testProxy.searchboxHandler.getCallCount('queryAutocomplete'));
      });

  test('matches cleared when new autochip added', async () => {
    createComposeboxElement(testProxy);
    await microtasksFinished();

    testProxy.searchboxHandler.reset();
    testProxy.searchboxHandler.setPromiseResolveFor(
        ADD_TAB_CONTEXT_FN, {low: BigInt(1), high: BigInt(2)});

    const tab = {
      tabId: 1,
      title: 'Tab 1',
      url: 'https://example.com/1',
      showInCurrentTabChip: true,
      showInPreviousTabChip: false,
      lastActive: {internalValue: BigInt(1)},
    } as any as TabInfo;

    // Add valid autochip.
    testProxy.searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(tab);
    await microtasksFinished();

    // Should clear matches when a new autochip is added.
    assertEquals(
        testProxy.searchboxHandler.getCallCount('stopAutocomplete'), 1);
  });

  test(
      'autocomplete not requeried if no autochip to start and updated with null',
      async () => {
        createComposeboxElement(testProxy);
        await microtasksFinished();

        // Autocomplete queried once on load.
        assertEquals(
            testProxy.searchboxHandler.getCallCount('queryAutocomplete'), 1);

        // Remove autochip when none exists.
        testProxy.searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
            null);
        await microtasksFinished();

        // Autocomplete should not be queried again when there was no autochip
        // to start, and an update comes with a null tab.
        assertEquals(
            testProxy.searchboxHandler.getCallCount('queryAutocomplete'), 1);
        assertEquals(
            testProxy.searchboxHandler.getCallCount('stopAutocomplete'), 0);
      });

  test('when flag enabled, adds tab context of ghost file', async () => {
    createComposeboxElement(testProxy);
    testProxy.element.shouldShowGhostFiles = true;

    await addTab();

    await testProxy.element.updateComplete;
    await microtasksFinished();

    assertTrue(
        testProxy.element.getNumOfFilesForTesting() === 1,
        'Tab should be added');

    const bad_token = FAKE_TOKEN_STRING_2;
    testProxy.searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        bad_token,
        FileUploadStatus.kUploadSuccessful,
        null,
    );
    await testProxy.element.updateComplete;
    await microtasksFinished();
    assertTrue(
        testProxy.element.getNumOfFilesForTesting() === 2,
        'Ghost file should be added');
  });

  test('does not add tab context of ghost file', async () => {
    createComposeboxElement(testProxy);
    testProxy.element.shouldShowGhostFiles = false;

    await addTab();
    await testProxy.element.updateComplete;
    await microtasksFinished();


    assertTrue(
        testProxy.element.getNumOfFilesForTesting() === 1,
        'Tab should be added');
    const bad_token = FAKE_TOKEN_STRING_2;
    testProxy.searchboxCallbackRouterRemote.onContextualInputStatusChanged(
        bad_token,
        FileUploadStatus.kUploadSuccessful,
        null,
    );
    await testProxy.element.updateComplete;
    await microtasksFinished();
    assertTrue(
        testProxy.element.getNumOfFilesForTesting() === 1,
        'Ghost file should not be added');
  });

  test(
      'multiple auto active tab updates only adds one chip with latest title',
      async () => {
        createComposeboxElement(testProxy);
        await microtasksFinished();

        const tab1 = {
          tabId: 1,
          title: 'Tab 1',
          url: 'https://example.com/1',
          showInCurrentTabChip: true,
          showInPreviousTabChip: false,
          lastActive: {internalValue: BigInt(1)},
        } as any as TabInfo;

        const tab1Updated = {
          tabId: 1,
          title: 'Tab 1 Updated Unique XYZ',
          url: 'https://example.com/1',
          showInCurrentTabChip: true,
          showInPreviousTabChip: false,
          lastActive: {internalValue: BigInt(1)},
        } as any as TabInfo;

        let resolveAddTab: (value: any) => void;
        testProxy.searchboxHandler.setResultMapperFor(
            ADD_TAB_CONTEXT_FN, () => {
              return new Promise(resolve => {
                resolveAddTab = resolve;
              });
            });

        // First update.
        testProxy.searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
            tab1);
        await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        // Second update with same URL but different title.
        testProxy.searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
            tab1Updated);
        await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        // Resolve the first (and only) addTabContext call.
        const tokenValue = 'token-multiple';
        resolveAddTab!({token: tokenValue});
        await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        // Should only have one file added to carousel with the updated title.
        assertEquals(
            1, testProxy.searchboxHandler.getCallCount(ADD_TAB_CONTEXT_FN));
        assertEquals(1, testProxy.element.$.carousel.files.length);
        assertEquals(
            'Tab 1 Updated Unique XYZ',
            testProxy.element.$.carousel.files[0]!.name);
      });
});

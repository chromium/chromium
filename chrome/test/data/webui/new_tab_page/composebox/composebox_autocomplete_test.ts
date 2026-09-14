// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeboxElement} from 'chrome://new-tab-page/lazy_load.js';
import {InputType} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {ADD_FILE_CONTEXT_FN, ADD_TAB_CONTEXT_FN, FAKE_TOKEN_STRING, generateZeroId, MockInputState, setupComposeboxTest} from './test_support.js';

suite('CrComposeboxAutocompleteContextTest', () => {
  const testProxy = setupComposeboxTest();

  // TODO(crbug.com/535685540): Remove this suite and its tests from here once
  // `cr-composebox` element is no longer used. AutoChip is not used in
  // `ntp-composebox`, it could be related to contextual_tasks.
  function createCrComposeboxElement() {
    testProxy.element = new ComposeboxElement();
    document.body.appendChild(testProxy.element);
  }

  test('autocomplete queried when autochip removed', async () => {
    createCrComposeboxElement();
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
    };

    // Add autochip.
    testProxy.searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
        tab, null);
    await microtasksFinished();

    // Should have cleared matches.
    assertEquals(
        1, testProxy.searchboxHandler.getCallCount('stopAutocomplete'));

    // Remove autochip.
    testProxy.searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
        null, null);
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
          ...new MockInputState(),
          maxInputsByType: {
            [InputType.kBrowserTab]: 1,
            [InputType.kLensImage]: 3,
            [InputType.kLensFile]: 1,
          },
          maxTotalInputs: 3,
        };
        loadTimeData.overrideValues({
          composeboxShowZps: true,
          tabFaviconChipsToCoinsEnabled: false,
        });
        createCrComposeboxElement();
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
        };

        // Add autochip.
        const autochipToken = generateZeroId();
        testProxy.searchboxHandler.setPromiseResolveFor(
            ADD_TAB_CONTEXT_FN, {token: autochipToken});
        testProxy.searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
            tab, null);
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
          inputType: InputType.kLensFile,
          isDeletable: true,
          objectUrl: null,
          dataUrl: null,
          url: null,
          tabId: null,
          iconName: null,
          supportsUnimodal: true,
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
    createCrComposeboxElement();
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
    };

    // Add valid autochip.
    testProxy.searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
        tab, null);
    await microtasksFinished();

    // Should clear matches when a new autochip is added.
    assertEquals(
        testProxy.searchboxHandler.getCallCount('stopAutocomplete'), 1);
  });

  test(
      'autocomplete not requeried if no autochip to start and updated with ' +
          'null',
      async () => {
        createCrComposeboxElement();
        await microtasksFinished();

        // Autocomplete queried once on load.
        assertEquals(
            testProxy.searchboxHandler.getCallCount('queryAutocomplete'), 1);

        // Remove autochip when none exists.
        testProxy.searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
            null, null);
        await microtasksFinished();

        // Autocomplete should not be queried again when there was no
        // autochip to start, and an update comes with a null tab.
        assertEquals(
            testProxy.searchboxHandler.getCallCount('queryAutocomplete'), 1);
        assertEquals(
            testProxy.searchboxHandler.getCallCount('stopAutocomplete'), 0);
      });

  test(
      'multiple auto active tab updates only adds one chip with latest title',
      async () => {
        loadTimeData.overrideValues({
          composeboxShowZps: true,
          tabFaviconChipsToCoinsEnabled: false,
        });
        createCrComposeboxElement();
        await microtasksFinished();

        const tab1 = {
          tabId: 1,
          title: 'Tab 1',
          url: 'https://example.com/1',
          showInCurrentTabChip: true,
          showInPreviousTabChip: false,
          lastActive: {internalValue: BigInt(1)},
        };

        const tab1Updated = {
          tabId: 1,
          title: 'Tab 1 Updated Unique XYZ',
          url: 'https://example.com/1',
          showInCurrentTabChip: true,
          showInPreviousTabChip: false,
          lastActive: {internalValue: BigInt(1)},
        };

        let resolveAddTab: (value: {token: string}) => void;
        testProxy.searchboxHandler.setResultMapperFor(
            ADD_TAB_CONTEXT_FN, () => {
              return new Promise<{token: string}>(resolve => {
                resolveAddTab = resolve;
              });
            });

        // First update.
        testProxy.searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
            tab1, null);
        await testProxy.searchboxCallbackRouterRemote.$.flushForTesting();
        await microtasksFinished();

        // Second update with same URL but different title.
        testProxy.searchboxCallbackRouterRemote.updateAutoSuggestedTabContext(
            tab1Updated, null);
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

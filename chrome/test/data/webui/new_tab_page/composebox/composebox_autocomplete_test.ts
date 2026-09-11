// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeboxElement} from 'chrome://new-tab-page/lazy_load.js';
import {InputType} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertStyle} from '../test_support.js';

import {ADD_FILE_CONTEXT_FN, ADD_TAB_CONTEXT_FN, areMatchesShowing, createComposeboxElement, FAKE_TOKEN_STRING, generateZeroId, MockInputState, setupComposeboxTest} from './test_support.js';

enum Attributes {
  SELECTED = 'selected',
}

function setInputValue(inputElement: HTMLElement, value: string) {
  if (inputElement instanceof HTMLTextAreaElement ||
      inputElement instanceof HTMLInputElement) {
    inputElement.value = value;
  } else {
    inputElement.innerText = value;
  }
}

function getInputValue(inputElement: HTMLElement): string {
  if (inputElement instanceof HTMLTextAreaElement ||
      inputElement instanceof HTMLInputElement) {
    return inputElement.value;
  }
  return inputElement.innerText;
}

suite(`NewTabPageComposeboxAutocompleteDropdownTest`, () => {
  const testProxy = setupComposeboxTest();

  test('composebox does not show verbatim match', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowTypedSuggest: true});
    createComposeboxElement(testProxy);
    await microtasksFinished();

    // Add zps input.
    setInputValue(testProxy.element.getInputElement().inputElement, '');
    testProxy.element.getInputElement().inputElement.dispatchEvent(
        new Event('input'));

    const matches = [
      createSearchMatchForTesting(),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
    ];
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          queryId: testProxy.element.activeQueryId,
          matches: matches,
        }));
    assertTrue(await areMatchesShowing(
        testProxy.element, testProxy.searchboxCallbackRouterRemote));

    let matchEls = testProxy.element.$.matches.shadowRoot.querySelectorAll(
        'cr-composebox-match');
    assertEquals(2, matchEls.length);
    let matchEl = matchEls[0];
    assertTrue(!!matchEl);
    // First match shows for zps.
    assertStyle(matchEl, 'display', 'block');

    // Add typed input
    setInputValue(testProxy.element.getInputElement().inputElement, 'awesome');
    testProxy.element.getInputElement().inputElement.dispatchEvent(
        new Event('input'));
    const typedMatches = [
      createSearchMatchForTesting({allowedToBeDefaultMatch: true}),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
    ];
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          queryId: testProxy.element.activeQueryId,
          input: 'awesome',
          matches: typedMatches,
        }));
    assertTrue(await areMatchesShowing(
        testProxy.element, testProxy.searchboxCallbackRouterRemote));

    matchEls = testProxy.element.$.matches.shadowRoot.querySelectorAll(
        'cr-composebox-match');
    assertEquals(2, matchEls.length);
    matchEl = matchEls[0];
    assertTrue(!!matchEl);
    // Verbatim match does not show for typed suggest.
    assertStyle(matchEl, 'display', 'none');
  });
});

suite(`NewTabPageComposeboxAutocompleteKeyboardNavigationTest`, () => {
  const testProxy = setupComposeboxTest();

  test('arrow keys work for typed suggest', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowTypedSuggest: true});
    createComposeboxElement(testProxy);
    await microtasksFinished();

    // Add typed input.
    setInputValue(testProxy.element.getInputElement().inputElement, 'Test');
    testProxy.element.getInputElement().inputElement.dispatchEvent(
        new Event('input'));
    await microtasksFinished();

    const composeboxDropdown =
        testProxy.element.shadowRoot.querySelector<HTMLElement>('#matches');

    const matches = [
      createSearchMatchForTesting(
          {fillIntoEdit: 'hello world 1', allowedToBeDefaultMatch: true}),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 3'}),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 4'}),
    ];
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          queryId: testProxy.element.activeQueryId,
          matches: matches,
          input: 'Test',
        }));
    await microtasksFinished();

    // Dropdown should show for when matches are available.
    assertFalse(composeboxDropdown!.hidden);

    const matchEls = testProxy.element.$.matches.shadowRoot.querySelectorAll(
        'cr-composebox-match');
    assertEquals(4, matchEls.length);
    const matchEl = matchEls[0];
    // Verbatim match does not show for typed suggest.
    assertStyle(matchEl!, 'display', 'none');

    // Arrow down should do default action.
    const arrowDownEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowDown',
    });

    testProxy.element.getInputElement().inputElement.dispatchEvent(
        arrowDownEvent);
    await microtasksFinished();
    assertTrue(arrowDownEvent.defaultPrevented);

    // First SHOWN match (second match) is selected.
    assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
    assertEquals(
        'hello world 2',
        getInputValue(testProxy.element.getInputElement().inputElement));

    // Arrow down should do default action.
    const arrowUpEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowUp',
    });

    testProxy.element.getInputElement().inputElement.dispatchEvent(
        arrowUpEvent);
    await microtasksFinished();
    assertTrue(arrowUpEvent.defaultPrevented);
    // Last match gets selected when arrowing up from the first
    // shown match.
    assertTrue(matchEls[3]!.hasAttribute(Attributes.SELECTED));
    assertEquals(
        'hello world 4',
        getInputValue(testProxy.element.getInputElement().inputElement));

    // When arrowing up from last match, first SHOWN match should be
    // selected.
    testProxy.element.getInputElement().inputElement.dispatchEvent(
        arrowDownEvent);
    await microtasksFinished();
    assertTrue(arrowDownEvent.defaultPrevented);
    assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
    assertEquals(
        'hello world 2',
        getInputValue(testProxy.element.getInputElement().inputElement));
  });
});

suite(`NewTabPageComposeboxAutocompleteMatchRemovalTest`, () => {
  const testProxy = setupComposeboxTest();

  test('delete button removes match', async () => {
    loadTimeData.overrideValues({composeboxShowZps: true});
    createComposeboxElement(testProxy);
    await microtasksFinished();

    const matches = [
      createSearchMatchForTesting(),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
    ];
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          queryId: testProxy.element.activeQueryId,
          input: '',
          matches,
          suggestionGroupsMap: {},
        }));

    assertTrue(await areMatchesShowing(
        testProxy.element, testProxy.searchboxCallbackRouterRemote));

    const matchEls = testProxy.element.$.matches.shadowRoot.querySelectorAll(
        'cr-composebox-match');
    assertEquals(2, matchEls.length);
    const matchEl = matchEls[0];
    assertTrue(!!matchEl);

    const matchIndex = 0;
    const destinationUrl = 'http://google.com';
    matchEl.matchIndex = matchIndex;
    matchEl.match.destinationUrl = destinationUrl;

    // By pressing 'Enter' on the button.
    const keydownEvent = (new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'Enter',
    }));
    assertTrue(!!matchEl.$.remove);
    matchEl.$.remove.dispatchEvent(keydownEvent);
    assertTrue(keydownEvent.defaultPrevented);
    const keydownArgs =
        await testProxy.searchboxHandler.whenCalled('deleteAutocompleteMatch');
    await microtasksFinished();
    assertEquals(matchIndex, keydownArgs[0]);
    assertEquals(destinationUrl, keydownArgs[1]);
    assertEquals(
        1, testProxy.searchboxHandler.getCallCount('deleteAutocompleteMatch'));
    // Pressing the 'Enter' button doesn't accidentally trigger
    // navigation.
    assertEquals(0, testProxy.searchboxHandler.getCallCount('submitQuery'));
    testProxy.searchboxHandler.reset();
    testProxy.handler.reset();

    matchEl.$.remove.click();
    const clickArgs =
        await testProxy.searchboxHandler.whenCalled('deleteAutocompleteMatch');
    await microtasksFinished();
    assertEquals(matchIndex, clickArgs[0]);
    assertEquals(destinationUrl, clickArgs[1]);
    assertEquals(
        1, testProxy.searchboxHandler.getCallCount('deleteAutocompleteMatch'));
    // Clicking the button doesn't accidentally trigger navigation.
    assertEquals(0, testProxy.searchboxHandler.getCallCount('submitQuery'));
  });
});

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

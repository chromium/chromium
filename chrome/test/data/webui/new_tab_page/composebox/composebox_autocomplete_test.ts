// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {VoiceSearchAction} from 'chrome://new-tab-page/lazy_load.js';
import {InputType} from 'chrome://resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {ComposeboxVoiceSearchElement} from 'chrome://resources/cr_components/composebox/composebox_voice_search.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertStyle} from '../test_support.js';

import {ADD_FILE_CONTEXT_FN, ADD_TAB_CONTEXT_FN, areMatchesShowing, createComposeboxElement, FAKE_TOKEN_STRING, generateZeroId, getSubmitIcon, MockInputState, setupComposeboxTest} from './test_support.js';

enum Attributes {
  SELECTED = 'selected',
}

suite('NewTabPageComposeboxAutocompleteDropdownTest', () => {
  const testProxy = setupComposeboxTest();

  test('dropdown shows when suggestions enabled', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowTypedSuggest: true});
    createComposeboxElement(testProxy);
    await microtasksFinished();

    // Add zps input.
    testProxy.element.getInputElement().inputElement.value = '';
    testProxy.element.getInputElement().inputElement.dispatchEvent(
        new Event('input'));
    await microtasksFinished();

    const composeboxDropdown =
        testProxy.element.shadowRoot.querySelector<HTMLElement>('#matches');

    // Composebox dropdown should not show for no matches.
    assertTrue(composeboxDropdown!.hidden);

    const matches = [
      createSearchMatchForTesting(),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
    ];
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));
    await microtasksFinished();

    // Dropdown should show for when matches are available.
    assertFalse(composeboxDropdown!.hidden);
  });

  test('dropdown does not show for multiline input', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowTypedSuggest: true});
    createComposeboxElement(testProxy);
    await microtasksFinished();

    // Add typed input.
    testProxy.element.getInputElement().inputElement.value = 'Test';
    testProxy.element.getInputElement().inputElement.style.height = '64px';
    testProxy.element.getInputElement().inputElement.dispatchEvent(
        new Event('input'));
    await microtasksFinished();

    const composeboxDropdown =
        testProxy.element.shadowRoot.querySelector<HTMLElement>('#matches');

    const matches = [
      createSearchMatchForTesting(),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
    ];
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));
    await microtasksFinished();

    // Dropdown should show for when matches are not available.
    assertTrue(composeboxDropdown!.hidden);

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
    assertFalse(arrowDownEvent.defaultPrevented);
  });

  test('dropdown does not show with multiple context files', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowTypedSuggest: true});
    createComposeboxElement(testProxy);
    await microtasksFinished();

    // Add zps input.
    testProxy.element.getInputElement().inputElement.value = '';
    testProxy.element.getInputElement().inputElement.dispatchEvent(
        new Event('input'));
    await microtasksFinished();

    const composeboxDropdown =
        testProxy.element.shadowRoot.querySelector<HTMLElement>('#matches');

    // Add matches and verify dropdown shows.
    const matches = [
      createSearchMatchForTesting(),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
    ];
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));
    await microtasksFinished();
    assertFalse(composeboxDropdown!.hidden);

    // If multiple context files are added, the dropdown should hide.
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
    testProxy.element.addFileContextForTesting({
      uuid: FAKE_TOKEN_STRING + '2',
      name: 'foo2.jpg',
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
    assertTrue(composeboxDropdown!.hidden);
  });

  test('dropdown does not show when no typed suggestions enabled', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowTypedSuggest: false});
    createComposeboxElement(testProxy);
    await microtasksFinished();

    // Add zps input.
    testProxy.element.getInputElement().inputElement.value = '';
    testProxy.element.getInputElement().inputElement.dispatchEvent(
        new Event('input'));
    await microtasksFinished();

    const composeboxDropdown =
        testProxy.element.shadowRoot.querySelector<HTMLElement>('#matches');

    const matches = [
      createSearchMatchForTesting(),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
    ];
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));
    await microtasksFinished();

    // Dropdown should show for when matches are available.
    assertFalse(composeboxDropdown!.hidden);

    testProxy.element.getInputElement().inputElement.value = 'Hello';
    testProxy.element.getInputElement().inputElement.dispatchEvent(
        new Event('input'));
    await microtasksFinished();

    // Dropdown should not show for typed input when typed suggest is
    // disabled.
    assertTrue(composeboxDropdown!.hidden);
  });

  test('dropdown does not show for typed suggest with context', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowTypedSuggest: true});
    createComposeboxElement(testProxy);
    await microtasksFinished();

    // Add typed input.
    testProxy.element.getInputElement().inputElement.value = 'Test';
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
          matches: matches,
          input: 'Test',
        }));
    await microtasksFinished();

    // Dropdown should show for when matches are available.
    assertFalse(composeboxDropdown!.hidden);

    // If context files are added, the dropdown should no longer be visible.
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
    assertTrue(composeboxDropdown!.hidden);
  });

  test(
      'dropdown does not show for typed suggest with verbatim match only',
      async () => {
        loadTimeData.overrideValues(
            {composeboxShowZps: true, composeboxShowTypedSuggest: true});
        createComposeboxElement(testProxy);
        await microtasksFinished();

        // Add typed input.
        testProxy.element.getInputElement().inputElement.value = 'Test';
        testProxy.element.getInputElement().inputElement.dispatchEvent(
            new Event('input'));
        await microtasksFinished();

        const composeboxDropdown =
            testProxy.element.shadowRoot.querySelector<HTMLElement>('#matches');

        const matches = [
          createSearchMatchForTesting(),
        ];
        testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
            createAutocompleteResultForTesting({
              matches: matches,
              input: 'Test',
            }));
        await microtasksFinished();

        // Dropdown should not show when only the verbatim match is present.
        assertTrue(composeboxDropdown!.hidden);
      });

  test('composebox does not show verbatim match', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowTypedSuggest: true});
    createComposeboxElement(testProxy);
    await microtasksFinished();

    // Add zps input.
    testProxy.element.getInputElement().inputElement.value = '';
    testProxy.element.getInputElement().inputElement.dispatchEvent(
        new Event('input'));

    const matches = [
      createSearchMatchForTesting(),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
    ];
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
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
    testProxy.element.getInputElement().inputElement.value = 'awesome';
    testProxy.element.getInputElement().inputElement.dispatchEvent(
        new Event('input'));
    const typedMatches = [
      createSearchMatchForTesting({allowedToBeDefaultMatch: true}),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
    ];
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
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

  test('image verbatim match does not show', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowTypedSuggest: true});
    createComposeboxElement(testProxy);
    await microtasksFinished();

    // Add image context.
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

    const matches = [
      createSearchMatchForTesting({allowedToBeDefaultMatch: true}),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
    ];
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));
    assertTrue(await areMatchesShowing(
        testProxy.element, testProxy.searchboxCallbackRouterRemote));

    const matchEls = testProxy.element.$.matches.shadowRoot.querySelectorAll(
        'cr-composebox-match');
    assertEquals(2, matchEls.length);
    const matchEl = matchEls[0];
    assertTrue(!!matchEl);
    // Verbatim match does not show for image context.
    assertStyle(matchEl, 'display', 'none');
  });
});

suite('NewTabPageComposeboxAutocompleteKeyboardNavigationTest', () => {
  const testProxy = setupComposeboxTest();

  test('escape key behavior with suggestions', async () => {
    loadTimeData.overrideValues({composeboxShowZps: true});
    createComposeboxElement(testProxy);
    await microtasksFinished();

    const matches = [
      createSearchMatchForTesting(),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
    ];
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({matches}));
    await microtasksFinished();
    assertTrue(await areMatchesShowing(
        testProxy.element, testProxy.searchboxCallbackRouterRemote));

    // Case 1: closeOnEscape = false. Escape should clear the text when the
    // composebox has content.
    testProxy.element.closeOnEscape = false;
    const closePromise = eventToPromise('close-composebox', testProxy.element);
    let closed = false;
    closePromise.then(() => closed = true);

    testProxy.element.getInputElement().inputElement.value = 'test';
    testProxy.element.getInputElement().inputElement.dispatchEvent(
        new Event('input'));
    await microtasksFinished();

    testProxy.element.getInputElement().inputElement.dispatchEvent(
        new KeyboardEvent(
            'keydown', {key: 'Escape', bubbles: true, composed: true}));
    await microtasksFinished();

    assertEquals(testProxy.searchboxHandler.getCallCount('clearFiles'), 1);
    assertFalse(closed);
    assertEquals('', testProxy.element.getInputElement().inputElement.value);

    // Case 2: closeOnEscape = true. Escape should close the composebox.
    testProxy.element.closeOnEscape = true;
    const whenCloseComposebox =
        eventToPromise('close-composebox', testProxy.element);
    testProxy.element.getInputElement().inputElement.dispatchEvent(
        new KeyboardEvent(
            'keydown', {key: 'Escape', bubbles: true, composed: true}));
    await whenCloseComposebox;
    assertEquals(testProxy.searchboxHandler.getCallCount('clearFiles'), 2);
  });

  test('arrow keys work for typed suggest', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowTypedSuggest: true});
    createComposeboxElement(testProxy);
    await microtasksFinished();

    // Add typed input.
    testProxy.element.getInputElement().inputElement.value = 'Test';
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
        testProxy.element.getInputElement().inputElement.value);

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
        testProxy.element.getInputElement().inputElement.value);

    // When arrowing up from last match, first SHOWN match should be selected.
    testProxy.element.getInputElement().inputElement.dispatchEvent(
        arrowDownEvent);
    await microtasksFinished();
    assertTrue(arrowDownEvent.defaultPrevented);
    assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
    assertEquals(
        'hello world 2',
        testProxy.element.getInputElement().inputElement.value);
  });

  test('arrow up/down moves selection / focus', async () => {
    loadTimeData.overrideValues({composeboxShowZps: true});
    createComposeboxElement(testProxy);
    await microtasksFinished();

    // Add zps input.
    testProxy.element.getInputElement().inputElement.value = '';
    testProxy.element.getInputElement().inputElement.dispatchEvent(
        new Event('input'));

    const matches = [
      createSearchMatchForTesting(),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
    ];
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));

    assertTrue(await areMatchesShowing(
        testProxy.element, testProxy.searchboxCallbackRouterRemote));

    const matchEls = testProxy.element.$.matches.shadowRoot.querySelectorAll(
        'cr-composebox-match');
    assertEquals(2, matchEls.length);

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

    // First match is selected
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals(
        'hello world', testProxy.element.getInputElement().inputElement.value);

    // Move the focus to the second match.
    matchEls[1]!.focus();
    matchEls[1]!.dispatchEvent(new Event('focusin', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
    }));
    await microtasksFinished();

    // Second match is selected and has focus.
    assertTrue(matchEls[1]!.hasAttribute(Attributes.SELECTED));
    assertEquals(
        'hello world 2',
        testProxy.element.getInputElement().inputElement.value);
    assertEquals(
        matchEls[1], testProxy.element.$.matches.shadowRoot.activeElement);

    const arrowUpEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'ArrowUp',
    });

    matchEls[1]!.dispatchEvent(arrowUpEvent);
    await microtasksFinished();
    assertTrue(arrowUpEvent.defaultPrevented);

    // First match gets selected and gets focus while focus is in the matches.
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals(
        'hello world', testProxy.element.getInputElement().inputElement.value);
    assertEquals(
        matchEls[0], testProxy.element.$.matches.shadowRoot.activeElement);

    // Restore.
    loadTimeData.overrideValues({composeboxShowZps: false});
  });

  test(
      'arrow up/down enables submit for suggestion with no query', async () => {
        loadTimeData.overrideValues({composeboxShowZps: true});
        createComposeboxElement(testProxy);
        await microtasksFinished();

        // Add zps input.
        testProxy.element.getInputElement().inputElement.value = '';
        testProxy.element.getInputElement().inputElement.dispatchEvent(
            new Event('input'));

        const matches = [
          createSearchMatchForTesting({fillIntoEdit: ''}),
        ];
        testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
            createAutocompleteResultForTesting({
              matches: matches,
            }));

        assertTrue(await areMatchesShowing(
            testProxy.element, testProxy.searchboxCallbackRouterRemote));

        const matchEls =
            testProxy.element.$.matches.shadowRoot.querySelectorAll(
                'cr-composebox-match');
        assertEquals(1, matchEls.length);

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

        // First match is selected
        assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
        assertEquals(
            '', testProxy.element.getInputElement().inputElement.value);

        // Assert submit is enabled.
        const submitButton = getSubmitIcon(testProxy);
        assertFalse(submitButton.hasAttribute('disabled'));

        // By pressing 'Enter' on the button.
        const keydownEvent = (new KeyboardEvent('keydown', {
          bubbles: true,
          cancelable: true,
          composed: true,
          key: 'Enter',
        }));
        matchEls[0]!.dispatchEvent(keydownEvent);
        assertTrue(keydownEvent.defaultPrevented);

        await microtasksFinished();

        // Assert call occurs.
        assertEquals(
            testProxy.searchboxHandler.getCallCount('openAutocompleteMatch'),
            1);

        // Restore.
        loadTimeData.overrideValues({composeboxShowZps: false});
      });
});

suite('NewTabPageComposeboxAutocompleteMatchRemovalTest', () => {
  const testProxy = setupComposeboxTest();

  test('Selection is restored after selected match is removed', async () => {
    loadTimeData.overrideValues(
        {composeboxShowZps: true, composeboxShowTypedSuggest: true});
    createComposeboxElement(testProxy);
    await microtasksFinished();

    testProxy.element.getInputElement().inputElement.value = '';
    testProxy.element.getInputElement().inputElement.dispatchEvent(
        new InputEvent('input'));

    let matches = [
      createSearchMatchForTesting({
        supportsDeletion: true,
      }),
    ];
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: testProxy.element.getInputElement()
                     .inputElement.value.trimStart(),
          matches,
        }));
    await microtasksFinished();
    assertTrue(await areMatchesShowing(
        testProxy.element, testProxy.searchboxCallbackRouterRemote));

    let matchEls = testProxy.element.$.matches.shadowRoot.querySelectorAll(
        'cr-composebox-match');
    assertEquals(1, matchEls.length);
    // First match is not selected.
    assertFalse(matchEls[0]!.hasAttribute(Attributes.SELECTED));

    // Remove the first match.
    matchEls[0]!.$.remove.click();
    const args =
        await testProxy.searchboxHandler.whenCalled('deleteAutocompleteMatch');
    assertEquals(0, args[0]);
    assertEquals(
        1, testProxy.searchboxHandler.getCallCount('deleteAutocompleteMatch'));

    testProxy.searchboxHandler.reset();

    matches = [
      createSearchMatchForTesting({supportsDeletion: true}),
      createSearchMatchForTesting({
        supportsDeletion: true,
        fillIntoEdit: 'hello world 2',
      }),
    ];
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: '',
          matches: matches,
        }));
    assertTrue(await areMatchesShowing(
        testProxy.element, testProxy.searchboxCallbackRouterRemote));

    matchEls = testProxy.element.$.matches.shadowRoot.querySelectorAll(
        'cr-composebox-match');
    assertEquals(2, matchEls.length);

    // First match is not selected.
    assertFalse(matchEls[0]!.hasAttribute(Attributes.SELECTED));

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

    // First match is selected
    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals(
        'hello world', testProxy.element.getInputElement().inputElement.value);

    // By pressing 'Enter' on the button.
    const keydownEvent = (new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'Enter',
    }));
    matchEls[0]!.$.remove.dispatchEvent(keydownEvent);
    assertTrue(keydownEvent.defaultPrevented);
    const keydownArgs =
        await testProxy.searchboxHandler.whenCalled('deleteAutocompleteMatch');
    await microtasksFinished();
    assertEquals(0, keydownArgs[0]);
    assertEquals(
        1, testProxy.searchboxHandler.getCallCount('deleteAutocompleteMatch'));

    matches = [createSearchMatchForTesting({
      supportsDeletion: true,
      fillIntoEdit: 'hello world 2',
    })];
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: '',
          matches: matches,
        }));
    assertTrue(await areMatchesShowing(
        testProxy.element, testProxy.searchboxCallbackRouterRemote));

    assertTrue(matchEls[0]!.hasAttribute(Attributes.SELECTED));
    assertEquals(
        'hello world 2',
        testProxy.element.getInputElement().inputElement.value);
  });

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
    // Pressing the 'Enter' button doesn't accidentally trigger navigation.
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

suite('NewTabPageComposeboxAutocompleteSmartComposeTest', () => {
  const testProxy = setupComposeboxTest();

  test('smart compose response added', async () => {
    createComposeboxElement(testProxy);
    await microtasksFinished();

    // Add input.
    testProxy.element.getInputElement().inputElement.value = 'smart ';
    testProxy.element.getInputElement().inputElement.dispatchEvent(
        new Event('input'));

    testProxy.element.haveReceivedSynchronousAutocompleteResponse = true;
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'smart ',
          matches: [],
          smartComposeInlineHint: 'compose',
        }));
    await microtasksFinished();

    assertEquals('compose', testProxy.element.smartComposeInlineHint);
  });

  test('tab adds smart compose to input', async () => {
    createComposeboxElement(testProxy);
    await microtasksFinished();
    // Autocomplete queried once when composebox is opened.
    assertEquals(
        testProxy.searchboxHandler.getCallCount('queryAutocomplete'), 1);

    // Add input.
    testProxy.element.getInputElement().inputElement.value = 'smart ';
    testProxy.element.getInputElement().inputElement.dispatchEvent(
        new Event('input'));

    // Autocomplete queried on input.
    assertEquals(
        testProxy.searchboxHandler.getCallCount('queryAutocomplete'), 2);

    testProxy.element.haveReceivedSynchronousAutocompleteResponse = true;
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'smart ',
          matches: [],
          smartComposeInlineHint: 'compose',
        }));
    await microtasksFinished();

    const tabEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,  // So it propagates across shadow DOM boundary.
      key: 'Tab',
    });

    testProxy.element.getInputElement().inputElement.dispatchEvent(tabEvent);
    await microtasksFinished();
    assertTrue(tabEvent.defaultPrevented);

    assertEquals(
        'smart compose',
        testProxy.element.getInputElement().inputElement.value);
    // Autocomplete queried when smart compose accepted.
    assertEquals(
        testProxy.searchboxHandler.getCallCount('queryAutocomplete'), 3);
  });

  test('arrow up/down moves clears smart compose', async () => {
    loadTimeData.overrideValues({composeboxShowTypedSuggest: true});
    createComposeboxElement(testProxy);
    await microtasksFinished();

    const matches = [
      createSearchMatchForTesting(),
      createSearchMatchForTesting({fillIntoEdit: 'hello world 2'}),
    ];

    // Add typed input
    testProxy.element.getInputElement().inputElement.value = 'awesome';
    testProxy.element.getInputElement().inputElement.dispatchEvent(
        new Event('input'));
    testProxy.element.haveReceivedSynchronousAutocompleteResponse = true;
    testProxy.searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'awesome',
          matches: matches,
          smartComposeInlineHint: 'compose',
        }));
    assertTrue(await areMatchesShowing(
        testProxy.element, testProxy.searchboxCallbackRouterRemote));

    const smartCompose =
        $$<HTMLElement>(testProxy.element.getInputElement(), '#smartCompose');
    assertTrue(!!smartCompose);

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

    assertFalse(!!$$<HTMLElement>(
        testProxy.element.getInputElement(), '#smartCompose'));
  });
});

suite('NewTabPageComposeboxAutocompleteQueryingTest', () => {
  const testProxy = setupComposeboxTest();

  test('composebox queries autocomplete on load', async () => {
    loadTimeData.overrideValues({composeboxShowZps: true});
    createComposeboxElement(testProxy);
    await microtasksFinished();

    // Autocomplete should be queried when the composebox is created.
    assertEquals(
        testProxy.searchboxHandler.getCallCount('queryAutocomplete'), 1);

    // Restore.
    loadTimeData.overrideValues({composeboxShowZps: false});
  });

  test('composebox stops autocomplete when clearing input', async () => {
    createComposeboxElement(testProxy);
    await microtasksFinished();

    // Autocomplete should be queried when the composebox is created.
    assertEquals(
        testProxy.searchboxHandler.getCallCount('queryAutocomplete'), 1);
    assertEquals(
        testProxy.searchboxHandler.getCallCount('stopAutocomplete'), 0);

    // Autocomplete complete should be queried when input is typed.
    testProxy.element.getInputElement().inputElement.value = 'T';
    testProxy.element.getInputElement().inputElement.dispatchEvent(
        new Event('input'));
    await microtasksFinished();
    assertEquals(
        testProxy.searchboxHandler.getCallCount('queryAutocomplete'), 2);

    // Deleting to empty input should stop autocomplete before querying it
    // again.
    testProxy.element.getInputElement().inputElement.value = '';
    testProxy.element.getInputElement().inputElement.dispatchEvent(
        new Event('input'));
    await microtasksFinished();

    assertEquals(
        testProxy.searchboxHandler.getCallCount('stopAutocomplete'), 1);
    assertEquals(
        testProxy.searchboxHandler.getCallCount('queryAutocomplete'), 3);
  });
});

suite('NewTabPageComposeboxAutocompleteContextTest', () => {
  const testProxy = setupComposeboxTest();

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
    };

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
          ...new MockInputState(),
          maxInputsByType: {
            [InputType.kBrowserTab]: 1,
            [InputType.kLensImage]: 3,
            [InputType.kLensFile]: 1,
          },
          maxTotalInputs: 3,
        };
        loadTimeData.overrideValues(
            {composeboxShowZps: true, tabFaviconChipsToCoinsEnabled: false});
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
        };

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
    };

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

  test(
      'multiple auto active tab updates only adds one chip with latest title',
      async () => {
        loadTimeData.overrideValues(
            {composeboxShowZps: true, tabFaviconChipsToCoinsEnabled: false});
        createComposeboxElement(testProxy);
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

suite('NewTabPageComposeboxAutocompleteVoiceSearchTest', () => {
  const testProxy = setupComposeboxTest();
  test(
      'submits w/o querying autocomplete on voice search final result',
      async () => {
        // Set loadTimeData so that voice search does auto submit.
        loadTimeData.overrideValues({
          composeboxShowZps: true,  // For predictable queryAutocomplete count.
        });
        createComposeboxElement(testProxy, {showVoiceSearch: true});
        await microtasksFinished();
        testProxy.searchboxHandler.reset();

        const voiceSearchActionPromise =
            eventToPromise<CustomEvent<{value: VoiceSearchAction}>>(
                'voice-search-action', testProxy.element);
        const voiceQuery = 'hello';
        const voiceSearchElement = $$<ComposeboxVoiceSearchElement>(
            testProxy.element, 'cr-composebox-voice-search');
        assertTrue(!!voiceSearchElement);
        voiceSearchElement.dispatchEvent(new CustomEvent(
            'voice-search-final-result',
            {detail: voiceQuery, bubbles: true, composed: true}));

        // Assert event fired.
        const voiceSearchActionEvent = await voiceSearchActionPromise;
        assertEquals(
            VoiceSearchAction.QUERY_SUBMITTED,
            voiceSearchActionEvent.detail.value);
        await microtasksFinished();

        assertEquals(
            testProxy.searchboxHandler.getCallCount('queryAutocomplete'), 0);
        assertEquals(testProxy.searchboxHandler.getCallCount('submitQuery'), 1);
        assertEquals(
            voiceQuery,
            testProxy.searchboxHandler.getArgs('submitQuery')[0][0]);
      });
});

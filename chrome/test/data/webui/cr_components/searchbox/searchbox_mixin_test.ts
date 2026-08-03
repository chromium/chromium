// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/searchbox/searchbox_dropdown.js';
import 'chrome://resources/cr_components/searchbox/searchbox_input.js';

import {createAutocompleteMatch, createAutocompleteResultForTesting, createSearchMatchForTesting, SearchboxBrowserProxy} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {SearchboxDropdownElement} from 'chrome://resources/cr_components/searchbox/searchbox_dropdown.js';
import type {SearchboxInputElement} from 'chrome://resources/cr_components/searchbox/searchbox_input.js';
import type {SearchboxMatchElement} from 'chrome://resources/cr_components/searchbox/searchbox_match.js';
import {SearchboxMixin} from 'chrome://resources/cr_components/searchbox/searchbox_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {NavigationPredictor} from 'chrome://resources/mojo/components/omnibox/browser/omnibox.mojom-webui.js';
import type {AutocompleteMatch} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {SelectionLineState} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertIconMaskImageUrl, assertStyle, createClipboardEvent, createKeyboardEvent, createUrlMatch} from './searchbox_test_utils.js';
import {TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

const TestElementBase = SearchboxMixin(CrLitElement);

interface TestSearchboxMixinElement {
  $: {
    input: SearchboxInputElement,
    matches: SearchboxDropdownElement,
    inputWrapper: HTMLElement,
  };
}

class TestSearchboxMixinElement extends TestElementBase {
  static get is() {
    return 'test-searchbox-mixin';
  }

  override render() {
    return html`
      <div id="inputWrapper"
          @focusout="${this.onInputWrapperFocusout}"
          @keydown="${this.onInputWrapperKeydown}">
        <cr-searchbox-input id="input"
            searchbox-icon="search.svg"
            .result="${this.result}"
            .selectedMatch="${this.selectedMatch}"
            @input-focus-changed="${this.onInputFocusChanged}"
            @searchbox-input-text-updated="${this.onSearchboxInputTextUpdated}">
        </cr-searchbox-input>
        <cr-searchbox-dropdown id="matches"
            role="listbox"
            .result="${this.result}"
            .selectedMatchIndex="${this.selectedMatchIndex}"
            @match-focusin="${this.onMatchFocusin}"
            @selected-match-index-changed="${this.onSelectedMatchIndexChanged}">
        </cr-searchbox-dropdown>
      </div>
    `;
  }

  override getInputElement(): SearchboxInputElement {
    return this.$.input;
  }

  override getDropdownElement(): SearchboxDropdownElement {
    return this.$.matches;
  }

  override getWrapperElement(): HTMLElement {
    return this.$.inputWrapper;
  }

  override pageHandler() {
    return SearchboxBrowserProxy.getInstance().handler;
  }

  // Should be `true` if the searchbox should append .com to the query when
  // Ctrl+Enter is pressed. This should be `true` for omnibox and `false` for
  // other searchboxes.
  shouldAppendDotCom = false;

  override shouldAppendDotComOnCtrlEnter() {
    return this.shouldAppendDotCom;
  }
}

customElements.define(TestSearchboxMixinElement.is, TestSearchboxMixinElement);

function simulateUserTextInput(
    inputElement: SearchboxInputElement, value: string): Promise<void> {
  inputElement.inputElement.value = value;
  inputElement.inputElement.dispatchEvent(new InputEvent('input'));
  return microtasksFinished();
}

function createCalculatorMatch(modifiers: Partial<AutocompleteMatch>):
    AutocompleteMatch {
  return createAutocompleteMatch({
    isSearchType: true,
    contents: '2 + 3',
    contentsClass: [{offset: 0, style: 0}],
    description: '5',
    descriptionClass: [{offset: 0, style: 0}],
    destinationUrl: 'https://www.google.com/search?q=2+%2B+3',
    fillIntoEdit: '5',
    type: 'search-calculator-answer',
    iconPath: 'calculator_cr23.svg',
    ...modifiers,
  });
}

function verifyMatch(match: AutocompleteMatch, matchEl: SearchboxMatchElement) {
  assertEquals('option', matchEl.getAttribute('role'));
  const matchContents = match.answer ? match.answer.firstLine : match.contents;
  const matchDescription =
      match.answer ? match.answer.secondLine : match.description;
  const separatorText =
      (match.swapContentsAndDescription ? match.contents : match.description) ?
      loadTimeData.getString('searchboxSeparator') :
      '';
  const contents = matchEl.$.contents.textContent;
  const separator = matchEl.$.separator.textContent;
  const description = matchEl.$.description.textContent;
  const text = contents + separator + description;
  assertEquals(
      match.swapContentsAndDescription ?
          matchDescription + separatorText + matchContents :
          matchContents + separatorText + matchDescription,
      text);
}

suite('SearchboxMixinTest', () => {
  let element: TestSearchboxMixinElement;
  let testProxy: TestSearchboxBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);

    element = document.createElement('test-searchbox-mixin') as
        TestSearchboxMixinElement;
    document.body.appendChild(element);
  });

  test('autocomplete should not query for empty inputs', async () => {
    const inputElement = element.getInputElement();
    await simulateUserTextInput(inputElement, 'he');

    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    // Deleting a character still queries autocomplete.
    await simulateUserTextInput(inputElement, 'h');

    assertEquals(2, testProxy.handler.getCallCount('queryAutocomplete'));

    // Deleting a character does not query autocomplete for empty input.
    await simulateUserTextInput(inputElement, '');
    assertEquals(2, testProxy.handler.getCallCount('queryAutocomplete'));

    // Typing space does not query autocomplete.
    await simulateUserTextInput(inputElement, ' ');
    assertEquals(2, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test(
      'onInputFocusChanged queries autocomplete if dropdown is not visible',
      async () => {
        element.dropdownIsVisible = false;
        element.getInputElement().fire('input-focus-changed', {value: 'hello'});
        await microtasksFinished();
        const args = await testProxy.handler.whenCalled('queryAutocomplete');
        assertEquals(args.input, 'hello');

        testProxy.handler.reset();

        element.dropdownIsVisible = true;
        element.getInputElement().fire('input-focus-changed', {value: 'hello'});
        await microtasksFinished();
        assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
      });

  test(
      'arrow up/down keys query autocomplete when dropdown is not visible',
      async () => {
        element.dropdownIsVisible = false;
        const mockInput = element.getInputElement();
        mockInput.inputElement.value = '';

        element.getWrapperElement().dispatchEvent(
            createKeyboardEvent('ArrowDown'));
        await microtasksFinished();

        let args = await testProxy.handler.whenCalled('queryAutocomplete');
        assertEquals(args.input, '');
        assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

        testProxy.handler.reset();

        mockInput.inputElement.value = 'hello';
        element.getWrapperElement().dispatchEvent(
            createKeyboardEvent('ArrowUp'));
        await microtasksFinished();

        args = await testProxy.handler.whenCalled('queryAutocomplete');
        assertEquals(args.input, 'hello');
        assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

        testProxy.handler.reset();

        // Does not query autocomplete when dropdown is visible.
        element.dropdownIsVisible = true;
        element.getWrapperElement().dispatchEvent(
            createKeyboardEvent('ArrowDown'));
        await microtasksFinished();
        assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
      });

  test('mousedown on empty input queries zero-prefix suggestions', async () => {
    const mockInput = element.getInputElement();
    mockInput.inputElement.value = '';
    mockInput.inputElement.dispatchEvent(new MouseEvent(
        'mousedown', {button: 0, bubbles: true, composed: true}));
    await microtasksFinished();

    const args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, '');
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    // Show zero-prefix matches.
    element.dropdownIsVisible = true;
    element.result = createAutocompleteResultForTesting({
      matches: [createSearchMatchForTesting(), createUrlMatch()],
    });
    await microtasksFinished();

    // Arrow up/down keys do not query autocomplete when matches are showing.
    element.getWrapperElement().dispatchEvent(createKeyboardEvent('ArrowUp'));
    await microtasksFinished();
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('queryAutocomplete passes cursor position', async () => {
    const inputElement = element.getInputElement();
    inputElement.inputElement.value = 'hello';
    inputElement.inputElement.selectionStart = 3;
    inputElement.inputElement.selectionEnd = 3;

    element.queryAutocomplete(
        'hello', /*preventInlineAutocomplete=*/ false, /*isOnFocus=*/ false);

    const args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, 'hello');
    assertEquals(args.cursorPosition, 3);
  });

  test(
      'queryAutocomplete passes cursor position when input is out of sync',
      async () => {
        const inputElement = element.getInputElement();
        inputElement.inputElement.value = 'hello';
        inputElement.inputElement.selectionStart = 3;
        inputElement.inputElement.selectionEnd = 3;

        element.queryAutocomplete(
            'hello world', /*preventInlineAutocomplete=*/ false,
            /*isOnFocus=*/ false);

        const args = await testProxy.handler.whenCalled('queryAutocomplete');
        assertEquals(args.input, 'hello world');
        assertEquals(args.cursorPosition, 11);
      });

  test('clearing the input stops autocomplete', async () => {
    const inputElement = element.getInputElement();
    await simulateUserTextInput(inputElement, 'h');

    let args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, 'h');
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    await simulateUserTextInput(inputElement, '');

    args = await testProxy.handler.whenCalled('stopAutocomplete');
    assertTrue(args.clearResult);
  });

  test('stale autocomplete response is ignored', async () => {
    element.queryAutocomplete(
        'he', /*preventInlineAutocomplete=*/ false, /*isOnFocus=*/ false);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    const matches = [createSearchMatchForTesting(), createUrlMatch()];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId - 1,  // Simulate stale response.
      input: 'h',
      matches: matches,
    }));
    await microtasksFinished();

    assertFalse(element.dropdownIsVisible);
    assertEquals(null, element.result);
  });

  test(
      'stale response with matching input but different queryId is ignored',
      async () => {
        const mockInput = element.getInputElement();

        // Query 0: type 'a'
        await simulateUserTextInput(mockInput, 'a');
        assertEquals(0, element.activeQueryId);

        // Query 1: type 'b'
        await simulateUserTextInput(mockInput, 'ab');
        assertEquals(1, element.activeQueryId);

        // Query 2: backspace to 'a'
        await simulateUserTextInput(mockInput, 'a');
        assertEquals(2, element.activeQueryId);

        // Receive results for query 0. Even though `lastQueriedInput` matches,
        // it should be discarded as stale.
        const matches = [createSearchMatchForTesting(), createUrlMatch()];
        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: 0,
          input: 'a',
          matches: matches,
        }));
        await microtasksFinished();

        // Check it's ignored.
        assertFalse(element.dropdownIsVisible);
        assertEquals(null, element.result);
      });

  test('arrow events are sent to handler', async () => {
    const inputElement = element.getInputElement();
    await simulateUserTextInput(inputElement, 'he');

    const matches = [createSearchMatchForTesting()];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'he',
      matches: matches,
    }));
    await microtasksFinished();
    assertTrue(element.dropdownIsVisible);

    const arrowDownEvent = createKeyboardEvent('ArrowDown');
    element.getWrapperElement().dispatchEvent(arrowDownEvent);
    await microtasksFinished();

    const args = await testProxy.handler.whenCalled('onNavigationLikely');
    assertEquals(0, args.line);
    assertEquals(
        NavigationPredictor.kUpOrDownArrowButton, args.navigationPredictor);
  });

  test('keyboard modifier keys behavior', async () => {
    const metaZEvent = createKeyboardEvent('z', {metaKey: true});
    const ctrlZEvent = createKeyboardEvent('z', {ctrlKey: true});

    let metaZStopped = false;
    let ctrlZStopped = false;
    metaZEvent.stopPropagation = () => {
      metaZStopped = true;
    };
    ctrlZEvent.stopPropagation = () => {
      ctrlZStopped = true;
    };

    element.getWrapperElement().dispatchEvent(metaZEvent);
    element.getWrapperElement().dispatchEvent(ctrlZEvent);
    await microtasksFinished();

    assertEquals(isMac, metaZStopped);
    assertEquals(!isMac, ctrlZStopped);
    assertFalse(metaZEvent.defaultPrevented);
    assertFalse(ctrlZEvent.defaultPrevented);
  });

  test('pressing Enter in empty input prevents new line', async () => {
    const mockInput = element.getInputElement();
    mockInput.inputElement.value = '';
    element.queryAutocomplete('', false, /*isOnFocus=*/ false);
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: '',
      matches: [createSearchMatchForTesting()],
    }));
    await microtasksFinished();

    const enterEvent = createKeyboardEvent('Enter');

    element.getWrapperElement().dispatchEvent(enterEvent);
    await microtasksFinished();

    assertTrue(enterEvent.defaultPrevented);
    assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));
  });

  test('onMatchFocusin selects match and updates input', async () => {
    const matches = [createSearchMatchForTesting({
      fillIntoEdit: 'test fill',
    })];
    element.result = createAutocompleteResultForTesting({
      matches: matches,
    });
    element.selectedMatchIndex = 0;
    element.selectedMatch = matches[0] || null;
    await microtasksFinished();

    element.getDropdownElement().fire('match-focusin', 0);
    await microtasksFinished();

    assertEquals(0, element.selectedMatchIndex);
    assertEquals('test fill', element.getInputElement().inputElement.value);
  });

  test(
      'onInputWrapperFocusout stops autocomplete or clears matches',
      async () => {
        const mockInput = element.getInputElement();
        mockInput.inputElement.value = 'hello';
        element.lastQueriedInput = 'hello';
        element.dropdownIsVisible = true;
        await microtasksFinished();

        // Focus stays inside wrapper.
        element.getWrapperElement().dispatchEvent(new FocusEvent('focusout', {
          relatedTarget: element.getInputElement().inputElement,
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();
        assertTrue(element.dropdownIsVisible);
        assertEquals(0, testProxy.handler.getCallCount('stopAutocomplete'));

        // Focus goes outside wrapper for non-empty input.
        element.getWrapperElement().dispatchEvent(new FocusEvent('focusout', {
          relatedTarget: document.body,
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();
        assertFalse(element.dropdownIsVisible);
        assertEquals(1, testProxy.handler.getCallCount('stopAutocomplete'));

        testProxy.handler.reset();

        // Focus goes outside wrapper for empty input.
        element.lastQueriedInput = '';
        element.dropdownIsVisible = true;
        element.getWrapperElement().dispatchEvent(new FocusEvent('focusout', {
          relatedTarget: document.body,
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();
        assertFalse(element.dropdownIsVisible);
        assertEquals(1, testProxy.handler.getCallCount('stopAutocomplete'));
      });

  test('pressing Enter on input navigates to the selected match', async () => {
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'hello ');

    const matches = [
      createSearchMatchForTesting({
        allowedToBeDefaultMatch: true,
        inlineAutocompletion: 'world',
      }),
      createUrlMatch(),
    ];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'hello ',
      matches: matches,
    }));
    await microtasksFinished();
    assertTrue(element.dropdownIsVisible);

    // Before navigation, input should be inline autocompleted.
    assertEquals('hello world', mockInput.inputElement.value);

    // Pressing Enter.
    const enterEvent = createKeyboardEvent('Enter');
    mockInput.inputElement.dispatchEvent(enterEvent);
    assertTrue(enterEvent.defaultPrevented);
    await microtasksFinished();

    const args = await testProxy.handler.whenCalled('openAutocompleteMatch');
    assertEquals(0, args.line);
    assertEquals(matches[0]!.destinationUrl, args.url);
    assertTrue(args.areMatchesShowing);
    assertTrue(args.viaKeyboard);
  });

  test('pressing Ctrl+Enter navigates to new match', async () => {
    element.shouldAppendDotCom = true;
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'hello');

    const matches = [
      createSearchMatchForTesting({
        allowedToBeDefaultMatch: true,
        fillIntoEdit: 'hello',
      }),
      createUrlMatch(),
    ];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'hello',
      matches: matches,
    }));
    await microtasksFinished();
    assertTrue(element.dropdownIsVisible);

    // Pressing Ctrl+Enter.
    const enterEvent = createKeyboardEvent('Enter', {ctrlKey: true});
    mockInput.inputElement.dispatchEvent(enterEvent);
    assertTrue(enterEvent.defaultPrevented);
    await microtasksFinished();

    const args = await testProxy.handler.whenCalled('openPopupSelection');
    assertEquals(0, args.selection.line);
    assertEquals(SelectionLineState.kCtrlEnter, args.selection.state);
  });

  test(
      'pressing Ctrl+Enter acts as normal enter on non-omnibox searchbox',
      async () => {
        element.shouldAppendDotCom = false;
        const mockInput = element.getInputElement();
        await simulateUserTextInput(mockInput, 'hello');

        const matches = [
          createSearchMatchForTesting({
            allowedToBeDefaultMatch: true,
            fillIntoEdit: 'hello',
          }),
          createUrlMatch(),
        ];
        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: element.activeQueryId,
          input: 'hello',
          matches: matches,
        }));
        await microtasksFinished();
        assertTrue(element.dropdownIsVisible);

        // Pressing Ctrl+Enter.
        const enterEvent = createKeyboardEvent('Enter', {ctrlKey: true});
        mockInput.inputElement.dispatchEvent(enterEvent);
        assertTrue(enterEvent.defaultPrevented);
        await microtasksFinished();

        // Since the feature is disabled, it should act as a normal Enter.
        const args =
            await testProxy.handler.whenCalled('openAutocompleteMatch');
        assertEquals(0, args.line);
        assertEquals(matches[0]!.destinationUrl, args.url);
        assertTrue(args.areMatchesShowing);
        assertTrue(args.viaKeyboard);
      });

  test('pressing Escape closes dropdown or resets input', async () => {
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'hello ');
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'hello ',
      matches: [createSearchMatchForTesting()],
    }));
    await microtasksFinished();
    assertTrue(element.dropdownIsVisible);

    const escapeEvent = createKeyboardEvent('Escape');
    element.getWrapperElement().dispatchEvent(escapeEvent);
    assertTrue(escapeEvent.defaultPrevented);
    await microtasksFinished();
    assertFalse(element.dropdownIsVisible);
  });

  test('Remove button is visible if the match supports deletion', async () => {
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'hello');

    const matches = [
      createSearchMatchForTesting(),
      createUrlMatch({supportsDeletion: true}),
    ];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'hello',
      matches: matches,
    }));
    await microtasksFinished();
    assertTrue(element.dropdownIsVisible);

    const matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(2, matchEls.length);
    assertEquals(
        window.getComputedStyle(matchEls[0]!.$.remove).display, 'none');

    matchEls[1]!.dispatchEvent(new Event('focusin', {
      bubbles: true,
      cancelable: true,
      composed: true,
    }));
    await microtasksFinished();
    assertNotEquals(
        window.getComputedStyle(matchEls[1]!.$.remove).display, 'none');
  });

  test('clicking remove button triggers deleteAutocompleteMatch', async () => {
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'hello');

    const matches = [createUrlMatch({supportsDeletion: true})];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'hello',
      matches: matches,
    }));
    await microtasksFinished();

    const matchEl = element.getDropdownElement().shadowRoot.querySelector(
        'cr-searchbox-match')!;
    matchEl.$.remove.click();

    const args = await testProxy.handler.whenCalled('deleteAutocompleteMatch');
    assertEquals(0, args.line);
    assertEquals(matches[0]!.destinationUrl, args.url);
  });

  // TODO(crbug.com/453570027): Test is flaky.
  test.skip('arrow up/down moves selection / focus', async () => {
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'hello');

    const matches = [createSearchMatchForTesting(), createUrlMatch()];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'hello',
      matches: matches,
    }));
    await microtasksFinished();
    assertTrue(element.dropdownIsVisible);

    const matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(2, matchEls.length);

    let arrowDownEvent = createKeyboardEvent('ArrowDown');
    mockInput.inputElement.dispatchEvent(arrowDownEvent);
    assertTrue(arrowDownEvent.defaultPrevented);
    await microtasksFinished();

    assertTrue(matchEls[0]!.hasAttribute('selected'));
    assertEquals('hello world', mockInput.inputElement.value);

    arrowDownEvent = createKeyboardEvent('ArrowDown');
    mockInput.inputElement.dispatchEvent(arrowDownEvent);
    assertTrue(arrowDownEvent.defaultPrevented);
    await microtasksFinished();

    assertTrue(matchEls[1]!.hasAttribute('selected'));
    assertEquals('https://helloworld.com', mockInput.inputElement.value);

    const arrowUpEvent = createKeyboardEvent('ArrowUp');
    mockInput.inputElement.dispatchEvent(arrowUpEvent);
    assertTrue(arrowUpEvent.defaultPrevented);
    await microtasksFinished();

    assertTrue(matchEls[0]!.hasAttribute('selected'));
    assertEquals('hello world', mockInput.inputElement.value);
  });

  test(
      'pressing Enter on matching suggestion navigates to destination',
      async () => {
        const mockInput = element.getInputElement();
        await simulateUserTextInput(mockInput, 'hello');

        const matches = [
          createSearchMatchForTesting({allowedToBeDefaultMatch: true}),
          createUrlMatch(),
        ];
        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: element.activeQueryId,
          input: 'hello',
          matches: matches,
        }));
        await microtasksFinished();
        assertTrue(element.dropdownIsVisible);

        const matchEl = element.getDropdownElement().shadowRoot.querySelector(
            'cr-searchbox-match')!;
        const enterEvent = createKeyboardEvent('Enter');
        matchEl.dispatchEvent(enterEvent);
        assertTrue(enterEvent.defaultPrevented);

        const args =
            await testProxy.handler.whenCalled('openAutocompleteMatch');
        assertEquals(0, args.line);
        assertEquals(matches[0]!.destinationUrl, args.url);
        assertTrue(args.viaKeyboard);
      });

  test('clicking suggestion triggers openAutocompleteMatch', async () => {
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'hello');

    const matches = [createSearchMatchForTesting(), createUrlMatch()];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'hello',
      matches: matches,
    }));
    await microtasksFinished();

    const matchEl = element.getDropdownElement().shadowRoot.querySelector(
        'cr-searchbox-match')!;
    matchEl.click();

    const args = await testProxy.handler.whenCalled('openAutocompleteMatch');
    assertEquals(0, args.line);
    assertEquals(matches[0]!.destinationUrl, args.url);
    assertEquals(0, args.mouseButton);
    assertFalse(args.viaKeyboard);
  });

  test('auxclick on suggestion triggers openAutocompleteMatch', async () => {
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'hello');

    const matches = [createSearchMatchForTesting(), createUrlMatch()];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'hello',
      matches: matches,
    }));
    await microtasksFinished();

    const matchEl = element.getDropdownElement().shadowRoot.querySelector(
        'cr-searchbox-match')!;
    const auxEvent = new MouseEvent('auxclick', {
      button: 1,
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    matchEl.dispatchEvent(auxEvent);
    assertTrue(auxEvent.defaultPrevented);

    const args = await testProxy.handler.whenCalled('openAutocompleteMatch');
    assertEquals(0, args.line);
    assertEquals(matches[0]!.destinationUrl, args.url);
    assertEquals(1, args.mouseButton);
    assertFalse(args.viaKeyboard);
  });

  test('clicking quick action pedal triggers executeAction', async () => {
    const matches = [
      createSearchMatchForTesting({
        actions: [{
          hint: 'Clear Browsing History',
          suggestionContents: '',
          iconPath: 'icon.png',
          a11yLabel: '',
        }],
      }),
      createSearchMatchForTesting({
        actions: [{
          hint: 'Open Email',
          suggestionContents: '',
          iconPath: 'icon.png',
          a11yLabel: '',
        }],
      }),
    ];
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'clear');
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'clear',
      matches: matches,
    }));
    await microtasksFinished();

    const matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    const actionEl1 =
        matchEls[1]!.shadowRoot.querySelector('cr-searchbox-action')!;

    const clickEvent = new MouseEvent('click', {
      bubbles: true,
      cancelable: true,
      composed: true,
      button: 1,
      altKey: true,
      ctrlKey: true,
      metaKey: true,
      shiftKey: true,
    });
    actionEl1.dispatchEvent(clickEvent);

    const args = await testProxy.handler.whenCalled('executeAction');
    assertEquals(1, args.line);
    assertEquals(0, args.actionIndex);
    assertEquals(1, args.mouseButton);
    assertTrue(args.altKey);
    assertTrue(args.ctrlKey);
    assertTrue(args.metaKey);
    assertTrue(args.shiftKey);
  });

  test('remove selected match using keyboard shortcut', async () => {
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'hello');

    const matches = [createUrlMatch({
      allowedToBeDefaultMatch: true,
      supportsDeletion: true,
    })];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'hello',
      matches: matches,
    }));
    await microtasksFinished();

    const matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(1, matchEls.length);
    assertTrue(matchEls[0]!.hasAttribute('selected'));

    mockInput.inputElement.dispatchEvent(
        createKeyboardEvent('Delete', {shiftKey: true}));

    const args = await testProxy.handler.whenCalled('deleteAutocompleteMatch');
    assertEquals(0, args.line);
    assertEquals(matches[0]!.destinationUrl, args.url);
  });

  test('selection is restored after selected match is removed', async () => {
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'hello');

    const matches = [
      createUrlMatch(
          {supportsDeletion: true, destinationUrl: 'https://url1.com'}),
      createUrlMatch(
          {supportsDeletion: true, destinationUrl: 'https://url2.com'}),
    ];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'hello',
      matches: matches,
    }));
    await microtasksFinished();

    let matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(2, matchEls.length);

    mockInput.inputElement.dispatchEvent(createKeyboardEvent('ArrowDown'));
    await microtasksFinished();
    assertTrue(matchEls[0]!.hasAttribute('selected'));

    matchEls[0]!.$.remove.click();
    await testProxy.handler.whenCalled('deleteAutocompleteMatch');

    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'hello',
      matches: [matches[1]!],
    }));
    await microtasksFinished();

    matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(1, matchEls.length);
    assertTrue(matchEls[0]!.hasAttribute('selected'));
  });

  test('renders rich suggestion answer and hides separator', async () => {
    const matches = [createSearchMatchForTesting({
      answer: {
        firstLine: 'When is Christmas Day',
        secondLine: 'Saturday, December 25, 2021',
      },
      isRichSuggestion: true,
    })];
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'When is Christmas Day');
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'When is Christmas Day',
      matches: matches,
    }));
    await microtasksFinished();

    const matchEl = element.getDropdownElement().shadowRoot.querySelector(
        'cr-searchbox-match')!;
    assertEquals(window.getComputedStyle(matchEl.$.separator).display, 'none');
  });

  test('autocomplete response', async () => {
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, '      hello world');
    const args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, mockInput.inputElement.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    const matches = [
      createSearchMatchForTesting({
        allowedToBeDefaultMatch: true,
      }),
      createUrlMatch(),
    ];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: mockInput.inputElement.value.trimStart(),
      matches: matches,
    }));
    await microtasksFinished();
    assertTrue(element.dropdownIsVisible);

    assertEquals('listbox', element.getDropdownElement().getAttribute('role'));
    const matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(2, matchEls.length);
    verifyMatch(matches[0]!, matchEls[0]!);
    verifyMatch(matches[1]!, matchEls[1]!);

    assertTrue(matchEls[0]!.hasAttribute('selected'));

    assertEquals('      hello world', mockInput.inputElement.value);
    const start = mockInput.inputElement.selectionStart!;
    const end = mockInput.inputElement.selectionEnd!;
    assertEquals('', mockInput.inputElement.value.substring(start, end));
  });

  test('autocomplete response with inline autocompletion', async () => {
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'hello ');
    let args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, mockInput.inputElement.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    const matches = [createSearchMatchForTesting({
      allowedToBeDefaultMatch: true,
      inlineAutocompletion: 'world',
    })];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: mockInput.inputElement.value.trimStart(),
      matches: matches,
    }));
    await microtasksFinished();
    assertTrue(element.dropdownIsVisible);

    const matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(1, matchEls.length);
    verifyMatch(matches[0]!, matchEls[0]!);

    assertTrue(matchEls[0]!.hasAttribute('selected'));

    assertEquals('hello world', mockInput.inputElement.value);
    let start = mockInput.inputElement.selectionStart!;
    let end = mockInput.inputElement.selectionEnd!;
    assertEquals('world', mockInput.inputElement.value.substring(start, end));

    let inputValueChanged = false;
    const originalValueProperty =
        Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value')!;
    Object.defineProperty(mockInput.inputElement, 'value', {
      get: originalValueProperty.get,
      set: (value) => {
        inputValueChanged = true;
        originalValueProperty.set!.call(mockInput.inputElement, value);
      },
    });

    const keyEvent = createKeyboardEvent('w');
    mockInput.inputElement.dispatchEvent(keyEvent);
    assertTrue(keyEvent.defaultPrevented);

    assertFalse(inputValueChanged);
    assertEquals('hello world', mockInput.inputElement.value);
    start = mockInput.inputElement.selectionStart!;
    end = mockInput.inputElement.selectionEnd!;
    assertEquals('orld', mockInput.inputElement.value.substring(start, end));

    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, 'hello w');
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('autocomplete response preserves cursor position', async () => {
    const mockInput = element.getInputElement();
    mockInput.inputElement.value = 'hello';
    mockInput.inputElement.selectionStart = 0;
    mockInput.inputElement.selectionEnd = 4;
    mockInput.inputElement.dispatchEvent(new InputEvent('input'));

    const matches = [createSearchMatchForTesting({
      allowedToBeDefaultMatch: true,
      contents: 'hello',
    })];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: mockInput.inputElement.value.trimStart(),
      matches: matches,
    }));
    await microtasksFinished();
    assertTrue(element.dropdownIsVisible);

    assertEquals('hello', mockInput.inputElement.value);
    const start = mockInput.inputElement.selectionStart;
    const end = mockInput.inputElement.selectionEnd;
    assertEquals('hell', mockInput.inputElement.value.substring(start, end));
  });

  test('autocomplete response changes', async () => {
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'he');

    const matches = [createSearchMatchForTesting(), createUrlMatch()];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: mockInput.inputElement.value.trimStart(),
      matches: matches,
    }));
    await microtasksFinished();
    assertTrue(element.dropdownIsVisible);

    let matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(2, matchEls.length);

    await simulateUserTextInput(mockInput, 'hell');
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: mockInput.inputElement.value.trimStart(),
    }));
    await microtasksFinished();
    assertFalse(element.dropdownIsVisible);

    matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(0, matchEls.length);

    await simulateUserTextInput(mockInput, 'hello');
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: mockInput.inputElement.value.trimStart(),
      matches: matches,
    }));
    await microtasksFinished();
    assertTrue(element.dropdownIsVisible);

    matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(2, matchEls.length);
  });

  // TODO(crbug.com/453570027): Test is flaky.
  test.skip('left-clicking the input queries autocomplete', async () => {
    const mockInput = element.getInputElement();
    mockInput.inputElement.value = '';
    mockInput.inputElement.dispatchEvent(new MouseEvent(
        'mousedown', {button: 0, bubbles: true, composed: true}));

    const args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals('', args.input);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    const matches = [createSearchMatchForTesting(), createUrlMatch()];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: '',
      matches: matches,
    }));
    await microtasksFinished();
    assertTrue(element.dropdownIsVisible);

    mockInput.inputElement.dispatchEvent(new MouseEvent(
        'mousedown', {button: 0, bubbles: true, composed: true}));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));

    // Right click does not query autocomplete.
    mockInput.inputElement.dispatchEvent(new MouseEvent(
        'mousedown', {button: 1, bubbles: true, composed: true}));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));

    element.clearAutocompleteMatches();
    await microtasksFinished();
    assertFalse(element.dropdownIsVisible);

    // Left click queries autocomplete when input is non-empty and dropdown
    // hidden.
    mockInput.inputElement.value = '   ';
    mockInput.inputElement.dispatchEvent(new MouseEvent(
        'mousedown', {button: 0, bubbles: true, composed: true}));
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('focusing the input does not query autocomplete', () => {
    const mockInput = element.getInputElement();
    mockInput.inputElement.value = '';
    mockInput.inputElement.focus();
    assertEquals(mockInput, element.shadowRoot.activeElement);
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('tabbing into empty input queries autocomplete', async () => {
    const mockInput = element.getInputElement();
    mockInput.inputElement.value = '';
    mockInput.inputElement.dispatchEvent(new MouseEvent(
        'mousedown', {button: 0, bubbles: true, composed: true}));
    let args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, mockInput.inputElement.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    const matches = [createSearchMatchForTesting(), createUrlMatch()];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      matches: matches,
    }));
    await microtasksFinished();
    assertTrue(element.dropdownIsVisible);

    const matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(2, matchEls.length);

    mockInput.inputElement.dispatchEvent(new KeyboardEvent('keyup', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'Tab',
    }));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));

    matchEls[0]!.dispatchEvent(new FocusEvent('focusout', {
      bubbles: true,
      cancelable: true,
      composed: true,
      relatedTarget: document.body,
    }));
    await microtasksFinished();

    mockInput.inputElement.dispatchEvent(new KeyboardEvent('keyup', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'Tab',
    }));
    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, mockInput.inputElement.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    mockInput.inputElement.value = '   ';
    mockInput.inputElement.dispatchEvent(new KeyboardEvent('keyup', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'Tab',
    }));
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('typing queries autocomplete', async () => {
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'he');

    let args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, mockInput.inputElement.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    mockInput.inputElement.value = 'h';
    mockInput.inputElement.dispatchEvent(new InputEvent('input'));

    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, mockInput.inputElement.value);
    assertTrue(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    await simulateUserTextInput(mockInput, 'he');

    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, mockInput.inputElement.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    const pasteEvent = createClipboardEvent('paste');
    mockInput.inputElement.dispatchEvent(pasteEvent);
    assertFalse(pasteEvent.defaultPrevented);
    mockInput.inputElement.value = 'hel';
    mockInput.inputElement.dispatchEvent(new InputEvent('input'));

    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, mockInput.inputElement.value);
    assertTrue(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    await simulateUserTextInput(mockInput, 'hell');

    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, mockInput.inputElement.value);
    assertFalse(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    mockInput.inputElement.value = 'hello';
    mockInput.inputElement.setSelectionRange(0, 0);
    mockInput.inputElement.dispatchEvent(new InputEvent('input'));

    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, mockInput.inputElement.value);
    assertTrue(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));

    testProxy.handler.reset();

    mockInput.inputElement.value = 'hello 간';
    mockInput.inputElement.dispatchEvent(
        new InputEvent('input', {isComposing: true}));

    args = await testProxy.handler.whenCalled('queryAutocomplete');
    assertEquals(args.input, mockInput.inputElement.value);
    assertTrue(args.preventInlineAutocomplete);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('match calculator answer type', async () => {
    const mockInput = element.getInputElement();
    const matches = [createCalculatorMatch({isRichSuggestion: true})];

    await simulateUserTextInput(mockInput, '2 + 3');

    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: mockInput.inputElement.value.trimStart(),
      matches: matches,
    }));
    await microtasksFinished();
    assertTrue(element.dropdownIsVisible);

    const matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(1, matchEls.length);

    verifyMatch(matches[0]!, matchEls[0]!);
    assertIconMaskImageUrl(matchEls[0]!.$.icon, 'calculator_cr23.svg');
    assertIconMaskImageUrl(mockInput.$.icon, 'search.svg');

    // Separator is not displayed
    assertEquals(
        window.getComputedStyle(matchEls[0]!.$.separator).display, 'none');

    const arrowDownEvent = createKeyboardEvent('ArrowDown');
    mockInput.inputElement.dispatchEvent(arrowDownEvent);
    assertTrue(arrowDownEvent.defaultPrevented);
    await microtasksFinished();

    assertTrue(matchEls[0]!.hasAttribute('selected'));
    assertEquals('5', mockInput.inputElement.value);

    assertIconMaskImageUrl(mockInput.$.icon, 'search.svg');
  });

  test('action with custom icon', async () => {
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'Open extension email');
    const matches = [
      createSearchMatchForTesting({
        actions: [{
          hint: 'Open Email',
          suggestionContents: '',
          iconPath: 'data:image/random',
          a11yLabel: '',
        }],
      }),
      createSearchMatchForTesting({
        actions: [{
          hint: 'Open Email',
          suggestionContents: '',
          iconPath: 'icon.png',
          a11yLabel: '',
        }],
      }),
    ];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: mockInput.inputElement.value.trimStart(),
      matches: matches,
    }));
    await microtasksFinished();
    assertTrue(element.dropdownIsVisible);

    const matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');

    // Match action that has a custom icon associated with it.
    const actionElCustomIcon =
        $$($$(matchEls[0]!, 'cr-searchbox-action')!, '.contents')!;
    const actionIconCustom =
        actionElCustomIcon.querySelector<HTMLElement>('#action-icon')!;
    // Match action that has a standard vector icon associated with it.
    const actionElStandardIcon =
        $$($$(matchEls[1]!, 'cr-searchbox-action')!, '.contents')!;
    const actionIconStandard =
        actionElStandardIcon.querySelector<HTMLElement>('#action-icon')!;

    // Custom icons should use `background-image` while standard vector icons
    // should use `-webkit-mask-image`.
    assertStyle(
        actionIconCustom, 'background-image', 'url("data:image/random")');
    assertStyle(
        actionIconStandard, '-webkit-mask-image',
        `url("${new URL('icon.png', document.baseURI).href}")`);
  });

  test(
      'pressing Enter on input navigates to hidden selected match',
      async () => {
        const mockInput = element.getInputElement();
        await simulateUserTextInput(mockInput, '  hello  ');

        const matches = [
          createSearchMatchForTesting({iconPath: 'clock.svg'}),
          createUrlMatch(),
        ];
        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: element.activeQueryId,
          input: mockInput.inputElement.value.trimStart(),
          matches: matches,
        }));
        await microtasksFinished();
        assertTrue(element.dropdownIsVisible);

        let matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
            'cr-searchbox-match');
        assertEquals(2, matchEls.length);

        matchEls[0]!.dispatchEvent(new Event('focusin', {
          bubbles: true,
          cancelable: true,
          composed: true,
        }));
        await microtasksFinished();

        assertTrue(matchEls[0]!.hasAttribute('selected'));
        assertEquals('hello world', mockInput.inputElement.value);
        assertIconMaskImageUrl(mockInput.$.icon, 'clock.svg');

        matchEls[0]!.dispatchEvent(new FocusEvent('focusout', {
          bubbles: true,
          cancelable: true,
          composed: true,
          relatedTarget: document.body,
        }));
        await microtasksFinished();
        assertFalse(element.dropdownIsVisible);

        matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
            'cr-searchbox-match');
        assertEquals(2, matchEls.length);
        assertTrue(matchEls[0]!.hasAttribute('selected'));
        assertEquals('hello world', mockInput.inputElement.value);
        assertIconMaskImageUrl(mockInput.$.icon, 'clock.svg');

        const shiftEnter = createKeyboardEvent('Enter', {shiftKey: true});
        mockInput.inputElement.dispatchEvent(shiftEnter);
        assertTrue(shiftEnter.defaultPrevented);

        const args =
            await testProxy.handler.whenCalled('openAutocompleteMatch');
        assertEquals(0, args.line);
        assertEquals(matches[0]!.destinationUrl, args.url);
        assertFalse(args.areMatchesShowing);
        assertTrue(args.modifiers.shiftKey);
        assertEquals(
            1, testProxy.handler.getCallCount('openAutocompleteMatch'));
      });

  test('pressing Enter on input is ignored if no selected match', async () => {
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'hello');

    const matches = [createSearchMatchForTesting(), createUrlMatch()];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: mockInput.inputElement.value.trimStart(),
      matches: matches,
    }));
    await microtasksFinished();
    assertTrue(element.dropdownIsVisible);

    const matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(2, matchEls.length);
    assertFalse(matchEls[0]!.hasAttribute('selected'));

    const shiftEnter = createKeyboardEvent('Enter', {shiftKey: true});
    mockInput.inputElement.dispatchEvent(shiftEnter);
    assertTrue(shiftEnter.defaultPrevented);

    assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));
  });

  test(
      'pressing Enter on input is ignored if no hidden selected match',
      async () => {
        const mockInput = element.getInputElement();
        mockInput.inputElement.value = '';
        mockInput.inputElement.dispatchEvent(new MouseEvent(
            'mousedown', {button: 0, bubbles: true, composed: true}));

        const matches = [
          createSearchMatchForTesting({iconPath: 'clock.svg'}),
          createUrlMatch(),
        ];
        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: element.activeQueryId,
          input: '',
          matches: matches,
        }));
        await microtasksFinished();
        assertTrue(element.dropdownIsVisible);

        let matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
            'cr-searchbox-match');
        assertEquals(2, matchEls.length);

        matchEls[0]!.dispatchEvent(new Event('focusin', {
          bubbles: true,
          cancelable: true,
          composed: true,
        }));
        await microtasksFinished();

        assertTrue(matchEls[0]!.hasAttribute('selected'));
        assertEquals('hello world', mockInput.inputElement.value);
        assertIconMaskImageUrl(mockInput.$.icon, 'clock.svg');

        matchEls[0]!.dispatchEvent(new FocusEvent('focusout', {
          bubbles: true,
          cancelable: true,
          composed: true,
          relatedTarget: document.body,
        }));
        await microtasksFinished();
        assertFalse(element.dropdownIsVisible);

        matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
            'cr-searchbox-match');
        assertEquals(0, matchEls.length);
        assertEquals('', mockInput.inputElement.value);
        assertIconMaskImageUrl(mockInput.$.icon, 'search.svg');

        const shiftEnter = createKeyboardEvent('Enter', {shiftKey: true});
        mockInput.inputElement.dispatchEvent(shiftEnter);
        assertFalse(shiftEnter.defaultPrevented);

        assertEquals(
            0, testProxy.handler.getCallCount('openAutocompleteMatch'));
      });

  test('pressing Enter on input too quickly', async () => {
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'hello');

    const matches = [
      createSearchMatchForTesting({allowedToBeDefaultMatch: true}),
      createUrlMatch(),
    ];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: mockInput.inputElement.value.trimStart(),
      matches: matches,
    }));
    await microtasksFinished();
    assertTrue(element.dropdownIsVisible);

    const matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    assertEquals(2, matchEls.length);
    assertTrue(matchEls[0]!.hasAttribute('selected'));

    await simulateUserTextInput(mockInput, 'hello world');
    const shiftEnter = createKeyboardEvent('Enter', {shiftKey: true});
    mockInput.inputElement.dispatchEvent(shiftEnter);
    assertTrue(shiftEnter.defaultPrevented);

    assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));

    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: mockInput.inputElement.value.trimStart(),
      matches: matches,
    }));
    await microtasksFinished();
    assertFalse(element.dropdownIsVisible);
    assertTrue(matchEls[0]!.hasAttribute('selected'));

    const args = await testProxy.handler.whenCalled('openAutocompleteMatch');
    assertEquals(0, args.line);
    assertEquals(matches[0]!.destinationUrl, args.url);
    assertTrue(args.areMatchesShowing);
    assertTrue(args.modifiers.shiftKey);
    assertTrue(args.viaKeyboard);
    assertEquals(1, testProxy.handler.getCallCount('openAutocompleteMatch'));
  });

  // TODO(crbug.com/453570027): Test is flaky.
  test.skip(
      'pressing Escape selects the first match / hides matches', async () => {
        const mockInput = element.getInputElement();
        await simulateUserTextInput(mockInput, 'hello');

        const matches = [createSearchMatchForTesting(), createUrlMatch()];
        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: element.activeQueryId,
          input: mockInput.inputElement.value.trimStart(),
          matches: matches,
        }));
        await microtasksFinished();
        assertTrue(element.dropdownIsVisible);

        let matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
            'cr-searchbox-match');
        assertEquals(2, matchEls.length);

        matchEls[1]!.focus();
        matchEls[1]!.dispatchEvent(new Event('focusin', {
          bubbles: true,
          cancelable: true,
          composed: true,
        }));
        await microtasksFinished();

        assertTrue(matchEls[1]!.hasAttribute('selected'));
        assertEquals('https://helloworld.com', mockInput.inputElement.value);
        assertEquals(
            matchEls[1], element.getDropdownElement().shadowRoot.activeElement);

        let escapeEvent = createKeyboardEvent('Escape');
        mockInput.inputElement.dispatchEvent(escapeEvent);
        assertTrue(escapeEvent.defaultPrevented);
        await microtasksFinished();

        assertTrue(matchEls[0]!.hasAttribute('selected'));
        assertEquals('hello world', mockInput.inputElement.value);
        assertEquals(
            matchEls[0], element.getDropdownElement().shadowRoot.activeElement);

        escapeEvent = createKeyboardEvent('Escape');
        mockInput.inputElement.dispatchEvent(escapeEvent);
        assertTrue(escapeEvent.defaultPrevented);
        await microtasksFinished();
        assertFalse(element.dropdownIsVisible);

        matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
            'cr-searchbox-match');
        assertEquals(0, matchEls.length);

        mockInput.inputElement.dispatchEvent(new MouseEvent(
            'mousedown', {button: 0, bubbles: true, composed: true}));
        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: element.activeQueryId,
          matches: matches,
        }));
        await microtasksFinished();
        assertTrue(element.dropdownIsVisible);

        matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
            'cr-searchbox-match');
        assertEquals(2, matchEls.length);

        escapeEvent = createKeyboardEvent('Escape');
        mockInput.inputElement.dispatchEvent(escapeEvent);
        assertTrue(escapeEvent.defaultPrevented);
        await microtasksFinished();
        assertFalse(element.dropdownIsVisible);

        matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
            'cr-searchbox-match');
        assertEquals(0, matchEls.length);
      });

  // TODO(crbug.com/453570027): Test is flaky.
  test.skip('focus indicator', async () => {
    const mockInput = element.getInputElement();
    mockInput.inputElement.focus();
    await simulateUserTextInput(mockInput, 'clear browsing history');

    const matches = [createSearchMatchForTesting({
      actions: [{
        hint: 'Clear Browsing History',
        suggestionContents: '',
        iconPath: 'chrome://theme/current-channel-logo',
        a11yLabel: '',
      }],
      fillIntoEdit: 'clear browsing history',
      supportsDeletion: true,
    })];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: mockInput.inputElement.value.trimStart(),
      matches: matches,
    }));
    await microtasksFinished();
    assertTrue(element.dropdownIsVisible);

    const matchEls = element.getDropdownElement().shadowRoot.querySelectorAll(
        'cr-searchbox-match');
    const focusIndicator = matchEls[0]!.$.focusIndicator;

    const arrowDownEvent = createKeyboardEvent('ArrowDown');
    mockInput.inputElement.dispatchEvent(arrowDownEvent);
    await microtasksFinished();

    assertTrue(matchEls[0]!.hasAttribute('selected'));
    assertEquals('clear browsing history', mockInput.inputElement.value);
    assertTrue(isVisible(focusIndicator));

    const action = $$<HTMLElement>(matchEls[0]!, '#action')!;
    action.focus();

    assertTrue(matchEls[0]!.hasAttribute('selected'));
    assertEquals(action, matchEls[0]!.shadowRoot.activeElement);
    assertFalse(isVisible(focusIndicator));

    const removeButton = matchEls[0]!.$.remove;
    removeButton.focus();

    assertTrue(matchEls[0]!.hasAttribute('selected'));
    assertEquals(removeButton, matchEls[0]!.shadowRoot.activeElement);
    assertFalse(isVisible(focusIndicator));
  });
});

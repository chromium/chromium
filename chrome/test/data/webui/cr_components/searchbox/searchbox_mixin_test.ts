// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/searchbox/searchbox_compose_button.js';
import 'chrome://resources/cr_components/searchbox/searchbox_dropdown.js';
import 'chrome://resources/cr_components/searchbox/searchbox_input.js';

import {createAutocompleteMatch, createAutocompleteResultForTesting, createMatchKeywordModelForTesting, createSearchMatchForTesting, SearchboxBrowserProxy} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {ComposeClickEventDetail} from 'chrome://resources/cr_components/searchbox/searchbox_compose_button.js';
import type {SearchboxDropdownElement} from 'chrome://resources/cr_components/searchbox/searchbox_dropdown.js';
import type {SearchboxInputElement} from 'chrome://resources/cr_components/searchbox/searchbox_input.js';
import type {SearchboxMatchElement} from 'chrome://resources/cr_components/searchbox/searchbox_match.js';
import {SearchboxMixin} from 'chrome://resources/cr_components/searchbox/searchbox_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {NavigationPredictor} from 'chrome://resources/mojo/components/omnibox/browser/omnibox.mojom-webui.js';
import type {AutocompleteMatch} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {KeywordType, SelectionLineState} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

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
            .inputKeywordModel="${this.inputKeywordModel}"
            @input-focus-changed="${this.onInputFocusChanged}"
            @searchbox-input-text-updated="${this.onSearchboxInputTextUpdated}">
        </cr-searchbox-input>
        <cr-searchbox-compose-button id="composeButton"
            slot="compose-button"
            ?has-virtual-focus="${this.isAiModeVirtualFocused()}"
            ?dropdown-is-visible="${this.dropdownIsVisible}"
            .virtualFocusEnabled="${this.virtualFocusEnabled}">
        </cr-searchbox-compose-button>
        <cr-searchbox-dropdown id="matches"
            role="listbox"
            .result="${this.result}"
            .selectedMatchIndex="${this.selectedMatchIndex}"
            .virtualFocusEnabled="${this.virtualFocusEnabled}"
            .selection="${this.selection}"
            @selection-changed="${this.onSelectionChanged}"
            @match-focusin="${this.onMatchFocusin}"
            @selected-match-index-changed="${this.onSelectedMatchIndexChanged}"
            @keyword-click="${this.onKeywordClick}">
        </cr-searchbox-dropdown>
      </div>
    `;
  }

  isAimButtonVisibleOverride = false;
  override get isAimButtonVisible(): boolean {
    return this.isAimButtonVisibleOverride;
  }

  showContextEntrypointOverride = false;
  override get showContextEntrypoint(): boolean {
    return this.showContextEntrypointOverride;
  }

  virtualFocusEnabledOverride = false;
  override get virtualFocusEnabled(): boolean {
    return this.virtualFocusEnabledOverride;
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
  const matchContents = match.contents;
  const matchDescription =
      match.description;
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

  test(
      'clicking remove button after interaction freeze unfreezes and accepts new results',
      async () => {
        const mockInput = element.getInputElement();
        await simulateUserTextInput(mockInput, 'hello');
        const queryId = element.activeQueryId;

        const matches = [
          createUrlMatch(
              {supportsDeletion: true, destinationUrl: 'https://first.com'}),
          createUrlMatch(
              {supportsDeletion: true, destinationUrl: 'https://second.com'}),
        ];
        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: queryId,
          input: 'hello',
          matches: matches,
        }));
        await microtasksFinished();
        assertEquals(2, element.result!.matches.length);

        // Freeze `activeQueryId` by simulating user interaction. New
        // autocomplete results for 'hello' will be ignored to avoid clobbering
        // user actions.
        const arrowDownEvent = createKeyboardEvent('ArrowDown');
        mockInput.inputElement.dispatchEvent(arrowDownEvent);
        await microtasksFinished();
        assertEquals(-1, element.activeQueryId);

        // Click remove button on the first match.
        const matchEl = element.getDropdownElement().shadowRoot.querySelector(
            'cr-searchbox-match')!;
        matchEl.$.remove.click();

        const args =
            await testProxy.handler.whenCalled('deleteAutocompleteMatch');
        assertEquals(0, args.line);
        assertEquals(matches[0]!.destinationUrl, args.url);

        // Backend sends updated results without the deleted match.
        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: queryId,
          input: 'hello',
          matches: [matches[1]!],
        }));
        await microtasksFinished();

        // The new results should be accepted despite the prior freeze.
        assertEquals(1, element.result!.matches.length);
        assertEquals(
            'https://second.com', element.result!.matches[0]!.destinationUrl);
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
    const matches = [createCalculatorMatch({isTwoRowSuggestion: true})];

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

  test('space-at-end keyword entry', async () => {
    const mockInput = element.getInputElement();
    const keyword = 'google.com';
    await simulateUserTextInput(mockInput, keyword);

    const matches = [createSearchMatchForTesting({
      allowedToBeDefaultMatch: true,
      keywordModel: createMatchKeywordModelForTesting({
        type: KeywordType.kChip,
        keyword,
        chipHint: 'Search Google',
      }),
    })];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: keyword,
      matches: matches,
    }));
    await microtasksFinished();

    await simulateUserTextInput(mockInput, keyword + ' ');

    assertTrue(element.inputKeywordModel !== null);
    assertEquals(KeywordType.kInKeyword, element.inputKeywordModel.type);
    assertEquals('', mockInput.inputElement.value);

    const keywordMatches = [createSearchMatchForTesting({
      allowedToBeDefaultMatch: true,
      keywordModel: createMatchKeywordModelForTesting({
        type: KeywordType.kInKeyword,
        keyword,
        chipHint: 'Search Google',
      }),
    })];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: '',
      matches: keywordMatches,
    }));
    await microtasksFinished();

    assertTrue(element.inputKeywordModel !== null);
    assertEquals(KeywordType.kInKeyword, element.inputKeywordModel.type);
  });

  test('question mark keyword entry', async () => {
    const mockInput = element.getInputElement();

    await simulateUserTextInput(mockInput, '?');

    assertTrue(element.inputKeywordModel !== null);
    assertEquals(KeywordType.kInKeyword, element.inputKeywordModel.type);
    assertEquals('?', element.inputKeywordModel.keyword);
    assertEquals('', mockInput.inputElement.value);

    const keywordMatches = [createSearchMatchForTesting({
      keywordModel: createMatchKeywordModelForTesting({
        type: KeywordType.kInKeyword,
        keyword: '?',
        chipHint: 'Search',
      }),
    })];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: '',
      matches: keywordMatches,
    }));
    await microtasksFinished();

    assertTrue(element.inputKeywordModel !== null);
    assertEquals(KeywordType.kInKeyword, element.inputKeywordModel.type);
  });

  test('chip click keyword entry', async () => {
    const mockInput = element.getInputElement();
    const keyword = 'google.com';

    const match = createSearchMatchForTesting({
      keywordModel: createMatchKeywordModelForTesting({
        type: KeywordType.kChip,
        keyword,
        chipHint: 'Search Google',
      }),
    });
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: keyword,
      matches: [match],
    }));
    await microtasksFinished();

    const dropdown = element.getDropdownElement();
    dropdown.dispatchEvent(new CustomEvent('keyword-click', {
      bubbles: true,
      composed: true,
      detail: {match, matchIndex: 0},
    }));
    await microtasksFinished();

    assertEquals(0, element.selection.line);
    assertEquals(SelectionLineState.kKeywordMode, element.selection.state);
    assertTrue(element.inputKeywordModel !== null);
    assertEquals(KeywordType.kInKeyword, element.inputKeywordModel.type);
    assertEquals(keyword, element.inputKeywordModel.keyword);
    assertEquals('Search Google', element.inputKeywordModel.displayText);
    assertEquals('', mockInput.inputElement.value);
  });

  // TODO(crbug.com/555945371): Fails on multiple OSes.
  test.skip(
      'navigating matches in keyword mode preserves keyword mode and icon',
      async () => {
        const mockInput = element.getInputElement();
        const keyword = 'google.com';

        const match0 = createSearchMatchForTesting({
          allowedToBeDefaultMatch: true,
          keywordModel: createMatchKeywordModelForTesting({
            type: KeywordType.kInKeyword,
            keyword,
            chipHint: 'Search Google',
          }),
        });
        const match1 = createUrlMatch({
          destinationUrl: 'https://youtube.com/',
        });

        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: element.activeQueryId,
          input: '',
          matches: [match0, match1],
        }));
        await microtasksFinished();

        element.inputKeywordModel = {
          type: KeywordType.kInKeyword,
          keyword,
          displayText: 'Search Google',
        };
        await microtasksFinished();
        await mockInput.updateComplete;
        await mockInput.$.icon.updateComplete;

        assertIconMaskImageUrl(
            mockInput.$.icon,
            '//resources/cr_components/searchbox/icons/search_cr23.svg');

        // Select the 2nd match (which does not have a keywordModel).
        element.selectedMatchIndex = 1;
        await microtasksFinished();
        await mockInput.updateComplete;
        await mockInput.$.icon.updateComplete;

        assertTrue(element.inputKeywordModel !== null);
        assertEquals(KeywordType.kInKeyword, element.inputKeywordModel.type);
        assertIconMaskImageUrl(
            mockInput.$.icon,
            '//resources/cr_components/searchbox/icons/search_cr23.svg');
      });

  test(
      'acceptInlineAutocomplete accepts text and queries autocomplete',
      async () => {
        const mockInput = element.getInputElement();
        mockInput.setInput({
          text: 'you',
          inline: 'tube.com',
        });
        await microtasksFinished();

        const tabEvent = new KeyboardEvent('keydown', {
          key: 'Tab',
          cancelable: true,
        });
        const handled = element.acceptInlineAutocomplete(tabEvent);
        assertTrue(handled);
        await microtasksFinished();

        assertTrue(tabEvent.defaultPrevented);
        assertEquals('youtube.com', mockInput.inputElement.value);
        assertEquals(11, mockInput.inputElement.selectionStart);
        assertEquals(11, mockInput.inputElement.selectionEnd);
        assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
        const args = await testProxy.handler.whenCalled('queryAutocomplete');
        assertEquals('youtube.com', args.input);
        assertFalse(args.preventInlineAutocomplete);
        assertEquals(11, args.cursorPosition);
        assertFalse(args.isOnFocus);
      });

  test(
      'acceptInlineAutocomplete with Shift clears inline text without preventDefault',
      async () => {
        const mockInput = element.getInputElement();
        mockInput.setInput({
          text: 'you',
          inline: 'tube.com',
        });
        await microtasksFinished();

        const shiftTabEvent = new KeyboardEvent('keydown', {
          key: 'Tab',
          shiftKey: true,
          cancelable: true,
        });
        const handled = element.acceptInlineAutocomplete(shiftTabEvent);
        assertTrue(handled);
        await microtasksFinished();

        assertFalse(shiftTabEvent.defaultPrevented);
        const lastInput = mockInput.lastInput();
        assertTrue(!!lastInput);
        assertEquals('', lastInput.inline);
        assertEquals('you', lastInput.text);
      });

  test(
      'acceptInlineAutocomplete returns false when no inline text exists',
      async () => {
        const mockInput = element.getInputElement();
        mockInput.setInput({
          text: 'youtube.com',
          inline: '',
        });
        await microtasksFinished();

        const tabEvent = new KeyboardEvent('keydown', {
          key: 'Tab',
          cancelable: true,
        });
        const handled = element.acceptInlineAutocomplete(tabEvent);
        assertFalse(handled);
        assertFalse(tabEvent.defaultPrevented);
        assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
      });

  test(
      'Tab key enters keyword mode when default match has keyword model',
      async () => {
        const mockInput = element.getInputElement();
        const keyword = 'google.com';

        const match = createSearchMatchForTesting({
          allowedToBeDefaultMatch: true,
          keywordModel: createMatchKeywordModelForTesting({
            type: KeywordType.kChip,
            keyword,
            chipHint: 'Search Google',
          }),
        });
        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: element.activeQueryId,
          input: 'google',
          matches: [match],
        }));
        await microtasksFinished();

        const event = new KeyboardEvent('keydown', {
          key: 'Tab',
          bubbles: true,
          cancelable: true,
        });
        element.handleKeyNavigation(event);
        await microtasksFinished();

        assertTrue(element.inputKeywordModel !== null);
        assertEquals(KeywordType.kInKeyword, element.inputKeywordModel.type);
        assertEquals(keyword, element.inputKeywordModel.keyword);
        assertEquals('', mockInput.inputElement.value);
        assertTrue(event.defaultPrevented);
      });

  test('Tab key on non-default match does not enter keyword mode', async () => {
    const defaultMatch = createSearchMatchForTesting({
      allowedToBeDefaultMatch: true,
      contents: 'google search',
    });
    const secondaryMatchWithKeyword = createSearchMatchForTesting({
      allowedToBeDefaultMatch: false,
      contents: 'google bookmarks',
      keywordModel: createMatchKeywordModelForTesting({
        type: KeywordType.kChip,
        keyword: 'google.com',
        chipHint: 'Search Google',
      }),
    });
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'google',
      matches: [defaultMatch, secondaryMatchWithKeyword],
    }));
    await microtasksFinished();

    // Select the second (non-default) match.
    await element.getDropdownElement().selectIndex(1);
    await microtasksFinished();

    const event = new KeyboardEvent('keydown', {
      key: 'Tab',
      bubbles: true,
      cancelable: true,
    });
    element.handleKeyNavigation(event);
    await microtasksFinished();

    assertFalse(
        element.inputKeywordModel !== null &&
        element.inputKeywordModel.type === KeywordType.kInKeyword);
    assertFalse(event.defaultPrevented);
  });

  test(
      'Tab key during IME composition does not enter keyword mode',
      async () => {
        const keyword = 'google.com';

        const match = createSearchMatchForTesting({
          allowedToBeDefaultMatch: true,
          keywordModel: createMatchKeywordModelForTesting({
            type: KeywordType.kChip,
            keyword,
            chipHint: 'Search Google',
          }),
        });
        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: element.activeQueryId,
          input: 'google',
          matches: [match],
        }));
        await microtasksFinished();

        const event = new KeyboardEvent('keydown', {
          key: 'Tab',
          bubbles: true,
          cancelable: true,
          isComposing: true,
        });
        element.handleKeyNavigation(event);
        await microtasksFinished();

        assertFalse(
            element.inputKeywordModel !== null &&
            element.inputKeywordModel.type === KeywordType.kInKeyword);
        assertFalse(event.defaultPrevented);
      });

  test('backspace at start exits keyword mode and restores text', async () => {
    const mockInput = element.getInputElement();
    const match = createSearchMatchForTesting({
      allowedToBeDefaultMatch: true,
      contents: 'youtube',
      fillIntoEdit: 'youtube.com',
      keywordModel: createMatchKeywordModelForTesting({
        type: KeywordType.kChip,
        keyword: 'youtube.com',
        chipHint: 'Search YouTube',
      }),
    });
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'youtube',
      matches: [match],
    }));
    await microtasksFinished();

    // Enter keyword mode via Tab key.
    const tabEvent = new KeyboardEvent('keydown', {
      key: 'Tab',
      bubbles: true,
      cancelable: true,
    });
    element.handleKeyNavigation(tabEvent);
    await microtasksFinished();

    assertTrue(element.keywordModeManager.isInKeywordMode);
    assertEquals('', mockInput.inputElement.value);

    // Press Backspace when input is empty.
    const backspaceEvent = new KeyboardEvent('keydown', {
      key: 'Backspace',
      bubbles: true,
      cancelable: true,
    });
    element.handleKeyNavigation(backspaceEvent);
    await microtasksFinished();

    assertFalse(element.keywordModeManager.isInKeywordMode);
    assertEquals('youtube.com', mockInput.inputElement.value);
    assertTrue(backspaceEvent.defaultPrevented);
  });

  test(
      'backspace at start after typing restores keyword with space and ' +
          'remaining text',
      async () => {
        const mockInput = element.getInputElement();
        const match = createSearchMatchForTesting({
          allowedToBeDefaultMatch: true,
          contents: 'youtube',
          fillIntoEdit: 'youtube.com',
          keywordModel: createMatchKeywordModelForTesting({
            type: KeywordType.kChip,
            keyword: 'youtube.com',
            chipHint: 'Search YouTube',
          }),
        });
        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: element.activeQueryId,
          input: 'youtube',
          matches: [match],
        }));
        await microtasksFinished();

        // Enter keyword mode via Tab key.
        const tabEvent = new KeyboardEvent('keydown', {
          key: 'Tab',
          bubbles: true,
          cancelable: true,
        });
        element.handleKeyNavigation(tabEvent);
        await microtasksFinished();

        // Type query 'q' in keyword mode.
        await simulateUserTextInput(mockInput, 'q');

        // Move cursor to start (selectionStart = 0).
        mockInput.inputElement.selectionStart = 0;
        mockInput.inputElement.selectionEnd = 0;

        // Press Backspace.
        const backspaceEvent = new KeyboardEvent('keydown', {
          key: 'Backspace',
          bubbles: true,
          cancelable: true,
        });
        element.handleKeyNavigation(backspaceEvent);
        await microtasksFinished();

        assertFalse(element.keywordModeManager.isInKeywordMode);
        assertEquals('youtube.com q', mockInput.inputElement.value);
        assertEquals(12, mockInput.inputElement.selectionStart);
        assertTrue(backspaceEvent.defaultPrevented);
      });
});

suite('SearchboxMixinVirtualFocusTest', () => {
  let element: TestSearchboxMixinElement;
  let testProxy: TestSearchboxBrowserProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);

    element = document.createElement('test-searchbox-mixin') as
        TestSearchboxMixinElement;
    element.virtualFocusEnabledOverride = true;
    document.body.appendChild(element);
    await microtasksFinished();
  });

  test('matchIndex returns selection line when virtual focus enabled', () => {
    element.virtualFocusEnabledOverride = true;

    // Line selection >= 0 returns selection.line directly.
    element.setSelection(
        {line: 2, state: SelectionLineState.kNormal, actionIndex: 0});
    assertEquals(2, element.matchIndex);

    // When selection line is -1 (input focused), falls back to default match.
    element.setSelection(
        {line: -1, state: SelectionLineState.kNormal, actionIndex: 0});
    element.result = createAutocompleteResultForTesting({
      matches: [createSearchMatchForTesting({allowedToBeDefaultMatch: true})],
    });
    assertEquals(0, element.matchIndex);

    element.result = createAutocompleteResultForTesting({
      matches: [createSearchMatchForTesting({allowedToBeDefaultMatch: false})],
    });
    assertEquals(-1, element.matchIndex);

    element.result = null;
    assertEquals(-1, element.matchIndex);

    // When virtual focus is disabled, uses selectedMatchIndex or default match
    // fallback.
    element.virtualFocusEnabledOverride = false;
    element.selectedMatchIndex = 3;
    assertEquals(3, element.matchIndex);

    element.selectedMatchIndex = -1;
    element.result = createAutocompleteResultForTesting({
      matches: [createSearchMatchForTesting({allowedToBeDefaultMatch: true})],
    });
    assertEquals(0, element.matchIndex);

    element.result = createAutocompleteResultForTesting({
      matches: [createSearchMatchForTesting({allowedToBeDefaultMatch: false})],
    });
    assertEquals(-1, element.matchIndex);
  });

  test(
      'ArrowDown and ArrowUp navigate selections in virtual focus mode',
      async () => {
        const mockInput = element.getInputElement();
        mockInput.inputElement.focus();
        await simulateUserTextInput(mockInput, 'hello');

        const matches = [
          createSearchMatchForTesting({
            allowedToBeDefaultMatch: false,
            fillIntoEdit: 'hello world',
            inlineAutocompletion: ' world',
          }),
          createSearchMatchForTesting({
            fillIntoEdit: 'hello there',
            destinationUrl: 'https://example.com/hello_there',
          }),
        ];

        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: element.activeQueryId,
          input: 'hello',
          matches: matches,
        }));
        await microtasksFinished();
        assertTrue(element.dropdownIsVisible);

        // ArrowDown navigates to first match (line 0).
        const arrowDownEvent = createKeyboardEvent('ArrowDown');
        mockInput.inputElement.dispatchEvent(arrowDownEvent);
        await microtasksFinished();

        assertEquals(0, element.selection.line);
        assertEquals(SelectionLineState.kNormal, element.selection.state);
        assertEquals('hello world', mockInput.inputElement.value);
        assertEquals(1, testProxy.handler.getCallCount('onNavigationLikely'));

        // ArrowDown navigates to second match (line 1).
        mockInput.inputElement.dispatchEvent(createKeyboardEvent('ArrowDown'));
        await microtasksFinished();

        assertEquals(1, element.selection.line);
        assertEquals('hello there', mockInput.inputElement.value);

        // ArrowUp navigates back to first match (line 0).
        mockInput.inputElement.dispatchEvent(createKeyboardEvent('ArrowUp'));
        await microtasksFinished();

        assertEquals(0, element.selection.line);
        assertEquals('hello world', mockInput.inputElement.value);

        // ArrowUp from first match (line 0) wraps around to the last match
        // (line 1).
        mockInput.inputElement.dispatchEvent(createKeyboardEvent('ArrowUp'));
        await microtasksFinished();

        assertEquals(1, element.selection.line);
        assertEquals('hello there', mockInput.inputElement.value);
      });

  test('PageDown and PageUp jump through selections', async () => {
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'test');

    const matches = [
      createSearchMatchForTesting({fillIntoEdit: 'test 1'}),
      createSearchMatchForTesting({fillIntoEdit: 'test 2'}),
      createSearchMatchForTesting({fillIntoEdit: 'test 3'}),
    ];

    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'test',
      matches: matches,
    }));
    await microtasksFinished();

    // PageDown navigates to the last match.
    mockInput.inputElement.dispatchEvent(createKeyboardEvent('PageDown'));
    await microtasksFinished();
    assertEquals(2, element.selection.line);

    // PageUp navigates to the first match.
    mockInput.inputElement.dispatchEvent(createKeyboardEvent('PageUp'));
    await microtasksFinished();
    assertEquals(0, element.selection.line);
  });

  test('Escape clears matches and resets input text', async () => {
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'test query');

    const matches = [createSearchMatchForTesting({fillIntoEdit: 'test query'})];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'test query',
      matches: matches,
    }));
    await microtasksFinished();
    assertTrue(element.dropdownIsVisible);

    // When unselected (line -1), a single Escape clears input and closes
    // dropdown.
    const escapeEvent = createKeyboardEvent('Escape');
    mockInput.inputElement.dispatchEvent(escapeEvent);
    await microtasksFinished();

    assertTrue(escapeEvent.defaultPrevented);
    assertEquals('', mockInput.inputElement.value);
    assertFalse(element.dropdownIsVisible);

    // Re-query and select match 1 to test unwinding.
    await simulateUserTextInput(mockInput, 'test query 2');
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'test query 2',
      matches: [
        createSearchMatchForTesting({fillIntoEdit: 'test query 2a'}),
        createSearchMatchForTesting({fillIntoEdit: 'test query 2b'}),
      ],
    }));
    await microtasksFinished();
    assertTrue(element.dropdownIsVisible);

    element.setSelection(
        {line: 1, state: SelectionLineState.kNormal, actionIndex: 0});
    await microtasksFinished();
    assertEquals(1, element.selection.line);

    // First Escape from navigated match jumps back to first match (line 0).
    let unwindEscape = createKeyboardEvent('Escape');
    mockInput.inputElement.dispatchEvent(unwindEscape);
    await microtasksFinished();
    assertEquals(0, element.selection.line);

    // Second Escape clears input and closes dropdown.
    unwindEscape = createKeyboardEvent('Escape');
    mockInput.inputElement.dispatchEvent(unwindEscape);
    await microtasksFinished();
    assertEquals('', mockInput.inputElement.value);
    assertFalse(element.dropdownIsVisible);
  });

  test('Tab exits natively on boundary', async () => {
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'tab test');

    const matches = [
      createSearchMatchForTesting({fillIntoEdit: 'tab test 1'}),
      createSearchMatchForTesting({fillIntoEdit: 'tab test 2'}),
    ];

    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'tab test',
      matches: matches,
    }));
    await microtasksFinished();

    // Select the last match (line 1).
    element.setSelection(
        {line: 1, state: SelectionLineState.kNormal, actionIndex: 0});
    await microtasksFinished();

    // Forward Tab from the last selection exits forward to kDefaultSelection.
    const tabEvent = createKeyboardEvent('Tab');
    mockInput.inputElement.dispatchEvent(tabEvent);
    await microtasksFinished();

    assertEquals(-1, element.selection.line);
    assertFalse(tabEvent.defaultPrevented);

    // Shift+Tab from index 0 resets selection to kDefaultSelection.
    const available = element.getAvailableSelections(element.result);
    if (available.length > 0) {
      element.setSelection(available[0]!);
      await microtasksFinished();
      const shiftTabEvent = createKeyboardEvent('Tab', {shiftKey: true});
      mockInput.inputElement.dispatchEvent(shiftTabEvent);
      await microtasksFinished();
      assertFalse(shiftTabEvent.defaultPrevented);
      assertEquals(-1, element.selection.line);
    }
  });

  test('Enter on focused AIM button fires compose-click event', async () => {
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'aim query');

    const matches = [createSearchMatchForTesting({fillIntoEdit: 'aim query'})];
    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'aim query',
      matches: matches,
    }));
    await microtasksFinished();

    element.setSelection({
      line: -1,
      state: SelectionLineState.kFocusedButtonAim,
      actionIndex: 0,
    });
    await microtasksFinished();

    let composeClicked = false;
    const composeButton =
        element.shadowRoot.querySelector('cr-searchbox-compose-button');
    assertTrue(!!composeButton);
    composeButton.addEventListener('compose-click', () => {
      composeClicked = true;
    });

    const enterEvent = createKeyboardEvent('Enter');
    mockInput.inputElement.dispatchEvent(enterEvent);
    await microtasksFinished();

    assertTrue(enterEvent.defaultPrevented);
    assertTrue(composeClicked);
  });

  test('Enter on focused action executes action', async () => {
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'action query');

    const matches = [createSearchMatchForTesting({
      actions: [{
        hint: 'Test Action',
        suggestionContents: '',
        iconPath: '',
        a11yLabel: '',
      }],
      fillIntoEdit: 'action query',
      destinationUrl: 'https://example.com/action',
    })];

    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'action query',
      matches: matches,
    }));
    await microtasksFinished();

    element.setSelection({
      line: 0,
      state: SelectionLineState.kFocusedButtonAction,
      actionIndex: 0,
    });
    await microtasksFinished();

    const enterEvent = createKeyboardEvent('Enter');
    mockInput.inputElement.dispatchEvent(enterEvent);
    await microtasksFinished();

    assertTrue(enterEvent.defaultPrevented);
    assertEquals(1, testProxy.handler.getCallCount('executeAction'));
    const args = await testProxy.handler.whenCalled('executeAction');
    assertEquals(0, args.line);
    assertEquals(0, args.actionIndex);
    assertEquals('https://example.com/action', args.url);
  });

  test(
      'Enter on remove suggestion button deletes match and unfreezes query ID',
      async () => {
        const mockInput = element.getInputElement();
        await simulateUserTextInput(mockInput, 'delete query');

        const matches = [createSearchMatchForTesting({
          fillIntoEdit: 'delete query',
          supportsDeletion: true,
          destinationUrl: 'https://example.com/delete',
        })];

        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: element.activeQueryId,
          input: 'delete query',
          matches: matches,
        }));
        await microtasksFinished();

        element.setSelection({
          line: 0,
          state: SelectionLineState.kFocusedButtonRemoveSuggestion,
          actionIndex: 0,
        });
        await microtasksFinished();

        const enterEvent = createKeyboardEvent('Enter');
        mockInput.inputElement.dispatchEvent(enterEvent);
        await microtasksFinished();

        assertTrue(enterEvent.defaultPrevented);
        assertEquals(
            1, testProxy.handler.getCallCount('deleteAutocompleteMatch'));
        const args =
            await testProxy.handler.whenCalled('deleteAutocompleteMatch');
        assertEquals(0, args.line);
        assertEquals('https://example.com/delete', args.url);
      });

  test(
      'post-deletion updates selection and fills input with sliding match',
      async () => {
        element.virtualFocusEnabledOverride = true;
        const mockInput = element.getInputElement();
        await simulateUserTextInput(mockInput, 'query');

        const match0 = createSearchMatchForTesting({
          fillIntoEdit: 'match 0',
          supportsDeletion: true,
          destinationUrl: 'https://example.com/0',
        });
        const match1 = createSearchMatchForTesting({
          fillIntoEdit: 'match 1',
          supportsDeletion: true,
          destinationUrl: 'https://example.com/1',
        });
        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: element.activeQueryId,
          matches: [match0, match1],
        }));
        await microtasksFinished();

        element.setSelection({
          line: 0,
          state: SelectionLineState.kFocusedButtonRemoveSuggestion,
          actionIndex: 0,
        });
        await microtasksFinished();

        // Simulate new result arriving after deletion where match 1 slides to
        // index 0.
        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: element.activeQueryId,
          matches: [match1],
        }));
        await microtasksFinished();

        // Selection should reset to kNormal on line 0.
        assertDeepEquals(element.selection, {
          line: 0,
          state: SelectionLineState.kNormal,
          actionIndex: 0,
        });
        // Input should be filled with sliding match text.
        assertEquals('match 1', mockInput.inputElement.value);
      });

  test(
      'Shift + Arrow keys do not trigger virtual focus navigation',
      async () => {
        element.virtualFocusEnabledOverride = true;
        const mockInput = element.getInputElement();
        await simulateUserTextInput(mockInput, 'query');

        const matches = [
          createSearchMatchForTesting({fillIntoEdit: 'match 0'}),
          createSearchMatchForTesting({fillIntoEdit: 'match 1'}),
        ];
        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: element.activeQueryId,
          matches: matches,
        }));
        await microtasksFinished();

        const shiftDownEvent =
            createKeyboardEvent('ArrowDown', {shiftKey: true});
        mockInput.inputElement.dispatchEvent(shiftDownEvent);
        await microtasksFinished();

        // Default should NOT be prevented, letting the native text input handle
        // selection.
        assertFalse(shiftDownEvent.defaultPrevented);
        // Selection should remain unselected (line -1).
        assertEquals(-1, element.selection.line);

        const shiftUpEvent = createKeyboardEvent('ArrowUp', {shiftKey: true});
        mockInput.inputElement.dispatchEvent(shiftUpEvent);
        await microtasksFinished();

        assertFalse(shiftUpEvent.defaultPrevented);
        assertEquals(-1, element.selection.line);
      });

  test('unfreezeActiveQueryId resets activeQueryId', () => {
    element.activeQueryId = 5;
    element.unfreezeActiveQueryId();
    assertEquals(-1, element.activeQueryId);
  });

  test('computeMatchFillIntoEdit strips keyword when in keyword mode', () => {
    element.inputKeywordModel = {
      type: KeywordType.kInKeyword,
      keyword: '@tabs',
      displayText: 'Tabs',
    };

    const match = createSearchMatchForTesting({
      fillIntoEdit: '@tabs search text',
    });

    const computed = element.computeMatchFillIntoEdit(match);
    assertEquals('search text', computed);
  });

  test(
      'Navigating off selections to line -1 restores lastQueriedInput',
      async () => {
        const mockInput = element.getInputElement();
        await simulateUserTextInput(mockInput, 'typed query');

        const matches = [createSearchMatchForTesting({
          fillIntoEdit: 'suggested query',
        })];

        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: element.activeQueryId,
          input: 'typed query',
          matches: matches,
        }));
        await microtasksFinished();

        // ArrowDown to match 0.
        mockInput.inputElement.dispatchEvent(createKeyboardEvent('ArrowDown'));
        await microtasksFinished();
        assertEquals(0, element.selection.line);
        assertEquals('suggested query', mockInput.inputElement.value);

        // Setting selection to line -1 and calling updateInputForSelection_
        // restores typed input.
        element.setSelection(
            {line: -1, state: SelectionLineState.kNormal, actionIndex: 0});
        await microtasksFinished();

        (element as unknown as {
          updateInputForSelection_: (s: unknown, k: string) => void,
        })
            .updateInputForSelection_(
                {line: -1, state: SelectionLineState.kNormal, actionIndex: 0},
                'Tab');
        await microtasksFinished();
        assertEquals('typed query', mockInput.inputElement.value);
      });

  test('Ctrl+Enter opens match via openCtrlEnterMatch', async () => {
    element.shouldAppendDotCom = true;
    const mockInput = element.getInputElement();
    await simulateUserTextInput(mockInput, 'ctrl enter query');

    const matches = [createSearchMatchForTesting({
      fillIntoEdit: 'ctrl enter query',
      destinationUrl: 'https://example.com/ctrl_enter',
    })];

    element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: element.activeQueryId,
      input: 'ctrl enter query',
      matches: matches,
    }));
    await microtasksFinished();

    element.setSelection(
        {line: 0, state: SelectionLineState.kNormal, actionIndex: 0});
    await microtasksFinished();

    const ctrlEnterEvent = createKeyboardEvent('Enter', {
      ctrlKey: true,
    });
    mockInput.inputElement.dispatchEvent(ctrlEnterEvent);
    await microtasksFinished();

    assertTrue(ctrlEnterEvent.defaultPrevented);
    const args = await testProxy.handler.whenCalled('openPopupSelection');
    assertEquals(0, args.selection.line);
    assertEquals(SelectionLineState.kCtrlEnter, args.selection.state);
  });

  test('isVirtualFocusEventTarget identifies valid targets', () => {
    const isVirtualFocusEventTarget =
        (element as unknown as {
          isVirtualFocusEventTarget_: (e: KeyboardEvent) => boolean,
        }).isVirtualFocusEventTarget_.bind(element);

    let inputResult = false;
    element.getInputElement().addEventListener(
        'keydown', (e: KeyboardEvent) => {
          inputResult = isVirtualFocusEventTarget(e);
        });
    element.getInputElement().dispatchEvent(createKeyboardEvent('Enter'));
    assertTrue(inputResult);

    let dropdownResult = false;
    element.getDropdownElement().addEventListener(
        'keydown', (e: KeyboardEvent) => {
          dropdownResult = isVirtualFocusEventTarget(e);
        });
    element.getDropdownElement().dispatchEvent(createKeyboardEvent('Enter'));
    assertTrue(dropdownResult);

    const composeButton =
        element.shadowRoot.querySelector('cr-searchbox-compose-button');
    if (composeButton) {
      let composeResult = false;
      composeButton.addEventListener('keydown', (e: KeyboardEvent) => {
        composeResult = isVirtualFocusEventTarget(e);
      });
      composeButton.dispatchEvent(createKeyboardEvent('Enter'));
      assertTrue(composeResult);
    }
  });

  test(
      'handleVirtualFocusEnter passes modifiers to compose-click', async () => {
        const mockInput = element.getInputElement();
        await simulateUserTextInput(mockInput, 'aim query');

        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: element.activeQueryId,
          input: 'aim query',
          matches: [createSearchMatchForTesting({fillIntoEdit: 'aim query'})],
        }));
        await microtasksFinished();

        element.setSelection({
          line: -1,
          state: SelectionLineState.kFocusedButtonAim,
          actionIndex: 0,
        });
        await microtasksFinished();

        const composeButton =
            element.shadowRoot.querySelector('cr-searchbox-compose-button');
        assertTrue(!!composeButton);
        const composeClickPromise =
            eventToPromise('compose-click', composeButton);

        const enterEvent = createKeyboardEvent('Enter', {
          ctrlKey: true,
          metaKey: false,
          shiftKey: true,
        });
        mockInput.inputElement.dispatchEvent(enterEvent);
        const clickEvent =
            await composeClickPromise as CustomEvent<ComposeClickEventDetail>;

        assertTrue(enterEvent.defaultPrevented);
        const eventDetail = clickEvent.detail;
        assertEquals(0, eventDetail.button);
        assertTrue(eventDetail.ctrlKey);
        assertFalse(eventDetail.metaKey);
        assertTrue(eventDetail.shiftKey);
      });

  test(
      'handleVirtualFocusEnter returns false for normal line states',
      async () => {
        element.setSelection({
          line: 0,
          state: SelectionLineState.kNormal,
          actionIndex: 0,
        });
        await microtasksFinished();

        const handled =
            (element as unknown as {
              handleVirtualFocusEnter_: (e: KeyboardEvent) => boolean,
            }).handleVirtualFocusEnter_(createKeyboardEvent('Enter'));
        assertFalse(handled);
      });

  test(
      'handleEnterNavigation ignores Shift+Enter when multiLineEnabled',
      async () => {
        element.multiLineEnabled = true;
        const mockInput = element.getInputElement();
        const shiftEnterEvent = createKeyboardEvent('Enter', {shiftKey: true});
        mockInput.inputElement.dispatchEvent(shiftEnterEvent);
        await microtasksFinished();

        assertFalse(shiftEnterEvent.defaultPrevented);
      });

  test(
      'updateInputForSelection only sets inline autocomplete for default ' +
          'match at line 0',
      async () => {
        const mockInput = element.getInputElement();
        await simulateUserTextInput(mockInput, 'test');

        const matches = [
          createSearchMatchForTesting({
            fillIntoEdit: 'test default',
            inlineAutocompletion: ' default',
            allowedToBeDefaultMatch: false,
          }),
          createSearchMatchForTesting({
            fillIntoEdit: 'test secondary',
            inlineAutocompletion: ' secondary',
            allowedToBeDefaultMatch: true,
          }),
        ];

        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: element.activeQueryId,
          input: 'test',
          matches: matches,
        }));
        await microtasksFinished();

        // Line 0 without allowedToBeDefaultMatch has no inline autocompletion.
        element.setSelection(
            {line: 0, state: SelectionLineState.kNormal, actionIndex: 0});
        await element.updateComplete;
        (element as unknown as {
          updateInputForSelection_: (s: unknown, k: string) => void,
        })
            .updateInputForSelection_(
                {line: 0, state: SelectionLineState.kNormal, actionIndex: 0},
                'ArrowDown');
        await microtasksFinished();
        assertEquals('test default', mockInput.inputElement.value);

        // Line 1 does not get inline autocompletion even if
        // allowedToBeDefaultMatch is true.
        element.setSelection(
            {line: 1, state: SelectionLineState.kNormal, actionIndex: 0});
        await element.updateComplete;
        (element as unknown as {
          updateInputForSelection_: (s: unknown, k: string) => void,
        })
            .updateInputForSelection_(
                {line: 1, state: SelectionLineState.kNormal, actionIndex: 0},
                'ArrowDown');
        await microtasksFinished();
        assertEquals('test secondary', mockInput.inputElement.value);
      });

  test(
      'handleKeyNavigation allows native Tab focus from ' +
          'contextual-entrypoint slot',
      async () => {
        const contextChip = document.createElement('button');
        contextChip.slot = 'contextual-entrypoint';
        element.$.inputWrapper.appendChild(contextChip);

        const tabEvent = new KeyboardEvent('keydown', {
          key: 'Tab',
          bubbles: true,
          composed: true,
          cancelable: true,
        });

        contextChip.dispatchEvent(tabEvent);
        await microtasksFinished();

        // The event should not be intercepted with preventDefault.
        assertFalse(tabEvent.defaultPrevented);
      });

  // TODO(https://crbug.com/555922132): de-flake and re-enable.
  test.skip(
      'ArrowDown through instant keyword mode matches enters keyword mode',
      async () => {
        loadTimeData.overrideValues({realboxVirtualFocusNavigation: true});
        element.virtualFocusEnabledOverride = true;

        const mockInput = element.getInputElement();
        await simulateUserTextInput(mockInput, '@');

        const defaultMatch = createSearchMatchForTesting({
          fillIntoEdit: '@',
          allowedToBeDefaultMatch: true,
        });
        const instantMatch = createSearchMatchForTesting({
          fillIntoEdit: '@bookmarks',
          allowedToBeDefaultMatch: false,
          keywordModel: createMatchKeywordModelForTesting({
            type: KeywordType.kInstant,
            keyword: '@bookmarks',
            chipHint: 'Bookmarks',
          }),
        });

        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: element.activeQueryId,
          input: '@',
          matches: [defaultMatch, instantMatch],
        }));
        await microtasksFinished();

        // ArrowDown to instant keyword match.
        mockInput.inputElement.dispatchEvent(createKeyboardEvent('ArrowDown'));
        await microtasksFinished();

        assertEquals(1, element.selection.line);
        assertEquals(SelectionLineState.kKeywordMode, element.selection.state);
        assertTrue(element.keywordModeManager.isInKeywordMode);
        assertEquals('@bookmarks', element.inputKeywordModel?.keyword);
        assertEquals('', mockInput.inputElement.value);

        // ArrowUp back to default search match.
        mockInput.inputElement.dispatchEvent(createKeyboardEvent('ArrowUp'));
        await microtasksFinished();

        assertEquals(0, element.selection.line);
        assertEquals(SelectionLineState.kNormal, element.selection.state);
        assertFalse(element.keywordModeManager.isInKeywordMode);
        assertEquals(null, element.inputKeywordModel);
        assertEquals('@', mockInput.inputElement.value);
      });

  test(
      'Tab key in Virtual Focus activates keyword mode on keyword chip',
      async () => {
        loadTimeData.overrideValues({realboxVirtualFocusNavigation: true});
        element.virtualFocusEnabledOverride = true;

        const mockInput = element.getInputElement();
        await simulateUserTextInput(mockInput, 'youtube.com');

        const defaultMatch = createSearchMatchForTesting({
          fillIntoEdit: 'youtube.com',
          allowedToBeDefaultMatch: true,
          keywordModel: createMatchKeywordModelForTesting({
            type: KeywordType.kChip,
            keyword: 'youtube.com',
            chipHint: 'Search YouTube',
          }),
        });

        element.onAutocompleteResultChanged(createAutocompleteResultForTesting({
          queryId: element.activeQueryId,
          input: 'youtube.com',
          matches: [defaultMatch],
        }));
        await microtasksFinished();

        // Tab selects the keyword chip on the default match.
        mockInput.inputElement.dispatchEvent(createKeyboardEvent('Tab'));
        await microtasksFinished();

        assertEquals(0, element.selection.line);
        assertEquals(SelectionLineState.kKeywordMode, element.selection.state);
        assertTrue(element.keywordModeManager.isInKeywordMode);
        assertEquals('youtube.com', element.inputKeywordModel?.keyword);
        assertEquals('', mockInput.inputElement.value);
      });
});

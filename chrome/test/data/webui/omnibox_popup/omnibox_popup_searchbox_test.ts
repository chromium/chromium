// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isMac} from '//resources/js/platform.js';
import {MatchesUpdate, OmniboxEscapeAction, omniboxPopupBrowserProxyFactory, OmniboxPopupPageHandlerRemote, sanitizeTextForPaste, SearchboxBrowserProxy, stripJavascriptSchemas, UrlDeemphasisMode} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import type {OmniboxInputState, OmniboxPopupContextualEntrypointButtonElement, OmniboxPopupPageRemote, OmniboxPopupSearchboxElement, UrlEmphasis} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {KeywordModeEntryMethod} from 'chrome://resources/cr_components/searchbox/keyword_mode_manager.js';
import {createAutocompleteResultForTesting, createMatchKeywordModelForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {KeywordType, RenderType, SelectionDirection, SelectionLineState, SelectionStep, SideType} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {createSuggestTemplateInfo} from 'chrome://webui-test/cr_components/searchbox/searchbox_test_utils.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createDefaultInputState, TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

function getContextualEntrypointButton(searchbox: OmniboxPopupSearchboxElement):
    OmniboxPopupContextualEntrypointButtonElement|null {
  const popupEntrypoint = $$(searchbox, 'omnibox-popup-contextual-entrypoint');
  if (!popupEntrypoint) {
    return null;
  }
  return $$<OmniboxPopupContextualEntrypointButtonElement>(
      popupEntrypoint, 'omnibox-popup-contextual-entrypoint-button');
}

function getLensSearchIcon(searchbox: OmniboxPopupSearchboxElement):
    (HTMLElement&{hasVirtualFocus: boolean})|null {
  const popupEntrypoint = $$(searchbox, 'omnibox-popup-contextual-entrypoint');
  if (!popupEntrypoint) {
    return null;
  }
  return $$<HTMLElement&{hasVirtualFocus: boolean}>(
      popupEntrypoint, '#lensSearchIcon');
}

function getCurrentTabChip(searchbox: OmniboxPopupSearchboxElement):
    (HTMLElement&{hasVirtualFocus: boolean})|null {
  const popupEntrypoint = $$(searchbox, 'omnibox-popup-contextual-entrypoint');
  if (!popupEntrypoint) {
    return null;
  }
  return $$<HTMLElement&{hasVirtualFocus: boolean}>(
      popupEntrypoint, '#currentTabChip');
}

function getLensSearchChip(searchbox: OmniboxPopupSearchboxElement):
    (HTMLElement&{hasVirtualFocus: boolean})|null {
  const popupEntrypoint = $$(searchbox, 'omnibox-popup-contextual-entrypoint');
  if (!popupEntrypoint) {
    return null;
  }
  return $$<HTMLElement&{hasVirtualFocus: boolean}>(
      popupEntrypoint, '#lensSearchChip');
}

function createDefaultOmniboxInputState(overrides?: Partial<OmniboxInputState>):
    OmniboxInputState {
  return {
    sequenceNumber: 1,
    tabId: 0,
    text: '',
    selection: {start: 0, end: 0},
    userInputInProgress: false,
    fullUrl: '',
    isFocused: false,
    permanentDisplayText: '',
    showFullUrl: false,
    matchesUpdate: MatchesUpdate.kDefault,
    keywordModel: null,
    isTabSwitch: false,
    urlEmphasis: null,
    unelidedUrlEmphasis: null,
    currentMatch: null,
    ...overrides,
  };
}

// Returns how far `offset` is from the start of `input`'s text, measured with
// the input's font. Only valid for LTR text.
function getTextOffsetLeft(
    input: HTMLInputElement|HTMLTextAreaElement, offset: number): number {
  const style = getComputedStyle(input);
  const context = document.createElement('canvas').getContext('2d')!;
  context.font = `${style.fontWeight} ${style.fontSize} ${style.fontFamily}`;
  return context.measureText(input.value.substring(0, offset)).width;
}

// Returns the width of the part of `input` that shows text, i.e. its content
// box. Doesn't use `clientWidth`: for text fields, it already excludes the
// inline padding while the InputClientRectWithoutInlinePadding Blink feature
// is enabled (the default), but includes it otherwise.
function getVisibleTextWidth(input: HTMLInputElement|HTMLTextAreaElement):
    number {
  const style = getComputedStyle(input);
  return input.offsetWidth - parseFloat(style.paddingLeft) -
      parseFloat(style.paddingRight) - parseFloat(style.borderLeftWidth) -
      parseFloat(style.borderRightWidth);
}

// Asserts that `offset` is inside the part of `input` that is scrolled into
// view, allowing 1px for rounding.
function assertTextOffsetVisible(
    input: HTMLInputElement|HTMLTextAreaElement, offset: number) {
  const visibleWidth = getVisibleTextWidth(input);
  const offsetLeft = getTextOffsetLeft(input, offset);
  assertTrue(
      offsetLeft >= input.scrollLeft - 1 &&
          offsetLeft <= input.scrollLeft + visibleWidth + 1,
      `offset at ${offsetLeft}px is outside the visible ${
          input.scrollLeft}px to ${input.scrollLeft + visibleWidth}px`);
}

const kDefaultSelection = {
  line: -1,
  state: SelectionLineState.kNormal,
  actionIndex: 0,
};

suite('OmniboxPopupSearchboxTest', function() {
  let searchbox: OmniboxPopupSearchboxElement;
  let testProxy: TestSearchboxBrowserProxy;
  let handler: TestMock<OmniboxPopupPageHandlerRemote>&
      OmniboxPopupPageHandlerRemote;
  let callbackRouter: OmniboxPopupPageRemote;

  suiteSetup(async () => {
    const link = document.createElement('link');
    link.rel = 'stylesheet';
    link.href = 'layout_constants_v0.css';
    const loaded = eventToPromise('load', link);
    document.head.appendChild(link);
    await loaded;
  });

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      hideClassicContextButton: false,
      composeboxShowContextMenuDescription: false,
      addContext: 'Add tabs and more',
      contextButtonShapeIsOblong: false,
      contextualMenuUsePecApi: false,
    });
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);
    handler = TestMock.fromClass(OmniboxPopupPageHandlerRemote);
    const {instance, remote} =
        omniboxPopupBrowserProxyFactory.createForTest(handler);
    callbackRouter = remote;
    omniboxPopupBrowserProxyFactory.setInstance(instance);
    searchbox = document.createElement('omnibox-popup-searchbox');
    document.body.appendChild(searchbox);
    await microtasksFinished();
  });

  test('HandlesSetInputState', async () => {
    // Set the input state via Mojo.
    const testText = 'test input';
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: testText,
    }));
    await microtasksFinished();

    // Ensure input element was updated with correct text and selection.
    const lastInput = searchbox.$.input.lastInput();
    assertTrue(!!lastInput);
    assertEquals(testText, lastInput.text);
    const input = searchbox.$.input.inputElement;
    assertEquals(0, input.selectionStart);
    assertEquals(0, input.selectionEnd);
    assertEquals(testText, searchbox.lastQueriedInput);
    assertEquals(-1, searchbox.selectedMatchIndex);
    assertFalse(searchbox.dropdownIsVisible);
    assertEquals(1, testProxy.handler.getCallCount('stopAutocomplete'));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));

    // Verify `MatchesUpdate.kQueryZps` initiates autocomplete query on focus.
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: 'zps query',
      matchesUpdate: MatchesUpdate.kQueryZps,
    }));
    await microtasksFinished();
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
    const [, , queryText, , , , isOnFocus] =
        testProxy.handler.getArgs('queryAutocomplete')[0];
    assertEquals('zps query', queryText);
    assertTrue(isOnFocus);
  });

  test('ResetsEditHistoryOnTabSwitch', async () => {
    // Initial state on Tab 1.
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      tabId: 1,
      text: 'tab 1 draft',
      userInputInProgress: true,
    }));
    await microtasksFinished();
    handler.reset();

    // Simulate typing in Tab 1 to create undo history.
    searchbox.$.input.fire(
        'searchbox-input-text-updated',
        {value: 'tab 1 draft edited', isComposing: false});
    await microtasksFinished();

    // Verify edit history has undoable edits on Tab 1.
    let [canUndo, canRedo] = await handler.whenCalled('setEditHistoryState');
    assertTrue(canUndo);
    assertFalse(canRedo);
    handler.resetResolver('setEditHistoryState');

    // Switch to Tab 2 with an in-progress draft.
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      tabId: 2,
      text: 'tab 2 draft',
      userInputInProgress: true,
      isTabSwitch: true,
    }));
    await microtasksFinished();

    // Tab 2 must have its edit history reset, preventing Tab 1 edits from
    // leaking.
    [canUndo, canRedo] = await handler.whenCalled('setEditHistoryState');
    assertFalse(canUndo);
    assertFalse(canRedo);
  });

  test('InFlightWebUiInputMergedWithSetInputState', async () => {
    // User typed 'e' in WebUI while SetInputState('h') was in-flight.
    const input = searchbox.$.input.inputElement;
    input.value = 'e';
    input.setSelectionRange(1, 1);
    input.dispatchEvent(new CustomEvent('input', {
      bubbles: true,
      composed: true,
      detail: {value: 'e', isComposing: false},
    }));
    await microtasksFinished();

    // Verify autocomplete query was deferred before initial SetInputState.
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));

    // SetInputState with text='h' arrives from native Views.
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: 'h',
      selection: {start: 1, end: 1},
      userInputInProgress: true,
      isFocused: true,
    }));
    await microtasksFinished();

    // Verify that the merged text is 'he' and selection is at end (2, 2).
    assertEquals('he', searchbox.$.input.inputElement.value);
    assertEquals(2, searchbox.$.input.inputElement.selectionStart);
    assertEquals(2, searchbox.$.input.inputElement.selectionEnd);

    // Verify autocomplete query was dispatched for the merged text.
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
    const [, , queryText] = testProxy.handler.getArgs('queryAutocomplete')[0];
    assertEquals('he', queryText);
  });

  test(
      'InFlightWebUiInputWithInlineAutocompletionMergedWithSetInputState',
      async () => {
        // User typed 'r' in WebUI.
        const input = searchbox.$.input.inputElement;
        input.value = 'r';
        input.setSelectionRange(1, 1);
        input.dispatchEvent(new CustomEvent('input', {
          bubbles: true,
          composed: true,
          detail: {value: 'r', isComposing: false},
        }));
        await microtasksFinished();

        // Autocomplete result arrives for 'r' with inline autocompletion
        // 'eddit.com'.
        testProxy.page.autocompleteResultChanged(
            createAutocompleteResultForTesting({
              input: 'r',
              matches: [
                createSearchMatchForTesting({
                  allowedToBeDefaultMatch: true,
                  inlineAutocompletion: 'eddit.com',
                  fillIntoEdit: 'reddit.com',
                }),
              ],
            }));
        await microtasksFinished();
        // Verify that raw typed 'r' was not overwritten by inline
        // autocompletion.
        assertEquals('r', searchbox.$.input.inputElement.value);

        // SetInputState with text='ca' arrives from native Views.
        callbackRouter.setInputState(createDefaultOmniboxInputState({
          text: 'ca',
          selection: {start: 2, end: 2},
          userInputInProgress: true,
          isFocused: true,
        }));
        await microtasksFinished();

        // Verify that only raw typed 'r' was merged with 'ca' to form 'car',
        // ignoring the inline autocompletion ('eddit.com').
        assertEquals('car', searchbox.$.input.inputElement.value);
        assertEquals(3, searchbox.$.input.inputElement.selectionStart);
        assertEquals(3, searchbox.$.input.inputElement.selectionEnd);
      });

  test(
      'InFlightWebUiInputPreservesLongerTextWhenDelayedSetInputStateArrives',
      async () => {
        // User already typed 'planet' in WebUI.
        const input = searchbox.$.input.inputElement;
        input.value = 'planet';
        input.setSelectionRange(6, 6);
        input.dispatchEvent(new CustomEvent('input', {
          bubbles: true,
          composed: true,
          detail: {value: 'planet', isComposing: false},
        }));
        await microtasksFinished();

        // Delayed SetInputState with text='plan' arrives from native Views.
        callbackRouter.setInputState(createDefaultOmniboxInputState({
          text: 'plan',
          selection: {start: 4, end: 4},
          userInputInProgress: true,
          isFocused: true,
        }));
        await microtasksFinished();

        // Verify that the longer WebUI input 'planet' is preserved and not
        // truncated back to 'plan'.
        assertEquals('planet', searchbox.$.input.inputElement.value);
        assertEquals(6, searchbox.$.input.inputElement.selectionStart);
        assertEquals(6, searchbox.$.input.inputElement.selectionEnd);
      });

  test('EnterKeySubmitsVerbatimMatchWhenNoMatchSelected', async () => {
    for (const virtualFocus of [false, true]) {
      loadTimeData.overrideValues(
          {realboxVirtualFocusNavigation: virtualFocus});
      searchbox.virtualFocusEnabled = virtualFocus;
      testProxy.handler.resetResolver('openAutocompleteMatch');

      callbackRouter.setInputState(createDefaultOmniboxInputState({
        text: 'chrome://version',
      }));
      await microtasksFinished();

      assertEquals(-1, searchbox.selectedMatchIndex);

      await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
        key: 'Enter',
        cancelable: true,
      }));
      await microtasksFinished();

      const [
        resultSequenceId,
        line,
        url,
        areMatchesShowing,
        mouseButton,
        modifiers,
        viaKeyboard,
      ] = await testProxy.handler.whenCalled('openAutocompleteMatch');
      assertEquals(0, resultSequenceId);
      assertEquals(-1, line);
      assertEquals('', url);
      assertFalse(areMatchesShowing);
      assertEquals(0, mouseButton);
      assertFalse(modifiers.altKey);
      assertFalse(modifiers.ctrlKey);
      assertFalse(modifiers.metaKey);
      assertFalse(modifiers.shiftKey);
      assertTrue(viaKeyboard);
    }
  });

  test('HandlesSelectionChange', async () => {
    // Focus the input so it's the active element.
    const input = searchbox.$.input.inputElement;
    input.focus();
    await microtasksFinished();
    // Set some text in the omnibox popup via Mojo.
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      sequenceNumber: 123,
      text: 'test text',
      userInputInProgress: true,
      isFocused: true,
    }));
    await microtasksFinished();

    // Send `focusin` event to clear `pendingFocusSelection_`.
    searchbox.$.input.dispatchEvent(new Event('focusin', {bubbles: true}));
    await new Promise(resolve => requestAnimationFrame(resolve));
    await microtasksFinished();

    // Set some selection in the HTML.
    input.setSelectionRange(1, 4);
    await microtasksFinished();

    // Ensure handler is notified of the selection change.
    const args = handler.getArgs('onSelectionChanged');
    const [selection, sequenceNumber] = args[args.length - 1];
    assertEquals(123, sequenceNumber);
    assertEquals(1, selection.start);
    assertEquals(4, selection.end);
  });

  test('IgnoresSelectionChangeWhenNotActive', async () => {
    // Ensure input isn't focused.
    const input = searchbox.$.input.inputElement;
    input.blur();
    await microtasksFinished();

    // Set some text and selection.
    input.value = 'test text';
    input.setSelectionRange(1, 4);
    document.dispatchEvent(new Event('selectionchange'));

    // Ensure handler wasn't notified of the non-active selection change.
    assertEquals(0, handler.getCallCount('onSelectionChanged'));
  });

  test('AppliesSelectionImmediately', async () => {
    // Set some input text and ensure it isn't focused.
    const input = searchbox.$.input.inputElement;
    input.value = 'test text';
    await microtasksFinished();
    input.blur();
    await microtasksFinished();

    // Set the input state via Mojo.
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: 'test text',
      selection: {start: 1, end: 4},
    }));
    await microtasksFinished();

    // Ensure selection was applied immediately regardless of focus.
    assertEquals(1, input.selectionStart);
    assertEquals(4, input.selectionEnd);
  });

  test('RejectsFocusWhenUserInputInProgress', async () => {
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: 'edited text',
      userInputInProgress: true,
      isFocused: true,
    }));
    await microtasksFinished();

    searchbox.onInputFocusChanged(new CustomEvent(
        'input-focus-changed',
        {detail: {value: 'edited text', isOnFocus: true}}));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
    assertFalse(searchbox.dropdownIsVisible);

    callbackRouter.setInputState(createDefaultOmniboxInputState({
      sequenceNumber: 2,
      text: 'permanent text',
      isFocused: true,
      permanentDisplayText: '',
    }));
    await microtasksFinished();

    searchbox.onInputFocusChanged(new CustomEvent(
        'input-focus-changed',
        {detail: {value: 'permanent text', isOnFocus: true}}));
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('IgnoresStaleAutocompleteResults', async () => {
    // Simulate user typing a custom query.
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: 'custom draft',
      selection: {start: 12, end: 12},
      userInputInProgress: true,
      isFocused: true,
    }));
    await microtasksFinished();

    // Send a stale autocomplete result (from an older query, e.g. "stale").
    testProxy.page.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'stale',
          matches: [
            createSearchMatchForTesting({
              allowedToBeDefaultMatch: true,
              fillIntoEdit: 'https://stale.com',
            }),
          ],
        }));
    await microtasksFinished();

    // Verify draft was protected (not overwritten) and dropdown remains closed.
    assertEquals('custom draft', searchbox.$.input.inputElement.value);
    assertFalse(searchbox.dropdownIsVisible);
  });

  // TODO(crbug.com/529516876): Fix and re-enable
  test.skip('SuppressesSelectionChangedDuringComposition', async () => {
    // Focus the input so it's the active element.
    const input = searchbox.$.input.inputElement;
    input.focus();
    await microtasksFinished();

    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: 'CJK text',
      userInputInProgress: true,
      isFocused: true,
    }));
    await microtasksFinished();
    handler.reset();

    // Send `focusin` event to clear `pendingFocusSelection_`.
    searchbox.$.input.dispatchEvent(new Event('focusin', {bubbles: true}));
    await microtasksFinished();

    // Start IME composition.
    searchbox.$.input.dispatchEvent(new CustomEvent('compositionstart'));
    await microtasksFinished();

    // Change selection while composing.
    input.setSelectionRange(1, 4);
    document.dispatchEvent(new Event('selectionchange'));
    await microtasksFinished();

    // Verify onSelectionChanged was suppressed.
    assertEquals(0, handler.getCallCount('onSelectionChanged'));

    // End IME composition.
    searchbox.$.input.dispatchEvent(new CustomEvent('compositionend'));
    await microtasksFinished();

    // Verify onSelectionChanged is sent once composition ends.
    assertEquals(1, handler.getCallCount('onSelectionChanged'));
  });

  test('DoubleClickingShowsFullUrl', async () => {
    // Focus the input.
    const input = searchbox.$.input.inputElement;
    const full_url = 'http://test.com';
    input.focus();
    await microtasksFinished();

    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: 'test.com',
      selection: {start: 0, end: 4},
      fullUrl: full_url,
      isFocused: true,
    }));
    await microtasksFinished();
    handler.reset();

    // Verify the full URL is displayed.
    assertEquals(full_url, input.value);
  });

  test('HandlesSetInputStateFocus', async () => {
    // Set isFocused = true.
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: 'test text',
      isFocused: true,
    }));
    await microtasksFinished();

    // Verify input element is focused.
    assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);

    // Set isFocused = false.
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      sequenceNumber: 2,
      text: 'test text',
      isFocused: false,
    }));
    await microtasksFinished();

    // Verify input element is blurred.
    assertFalse(searchbox.$.input === searchbox.shadowRoot.activeElement);
  });


  test('SubsequentSelectionChangesNotIgnoredAfterFocus', async () => {
    // Set the input state via Mojo with isFocused = true.
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: 'hello world',
      isFocused: true,
    }));
    await microtasksFinished();
    await microtasksFinished();

    // The input should be focused.
    const input = searchbox.$.input.inputElement;
    assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
    await new Promise(resolve => requestAnimationFrame(resolve));

    // Reset handler call count.
    handler.reset();

    // Simulate the user changing selection (e.g. by dragging/clicking)
    // inside the already focused input.
    input.setSelectionRange(1, 4);
    await microtasksFinished();

    // Check if the handler was notified.
    assertEquals(1, handler.getCallCount('onSelectionChanged'));
  });

  test('MousedownZeroLengthSelectionGuards', async () => {
    const input = searchbox.$.input.inputElement;
    input.value = '';
    await microtasksFinished();

    const mousedownEvent = new MouseEvent('mousedown', {
      bubbles: true,
      cancelable: true,
    });
    input.dispatchEvent(mousedownEvent);
    await microtasksFinished();

    assertEquals(0, input.selectionStart);
    assertEquals(0, input.selectionEnd);
    assertEquals('', input.value);
  });

  function dispatchMouseEvent(
      target: EventTarget, type: string, init: MouseEventInit = {}) {
    target.dispatchEvent(new MouseEvent(type, {
      button: 0,
      clientX: 20,
      clientY: 20,
      detail: 1,
      bubbles: true,
      cancelable: true,
      composed: true,
      ...init,
    }));
  }

  test('ClickTriggersZeroSuggestOnMouseUp', async () => {
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      sequenceNumber: 2,
      text: 'https://example.com',
      fullUrl: 'https://example.com',
      permanentDisplayText: 'example.com',
      isFocused: false,
    }));
    await microtasksFinished();
    testProxy.handler.reset();

    const input = searchbox.$.input.inputElement;
    dispatchMouseEvent(input, 'mousedown', {buttons: 1});
    // Simulate Blink focusing the input on mouse press.
    input.focus();
    await microtasksFinished();

    // Zero-prefix suggestions are deferred to mouse release, so that a drag
    // does not query them.
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));

    dispatchMouseEvent(document, 'mouseup', {buttons: 0});
    await microtasksFinished();

    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
    const args = testProxy.handler.getArgs('queryAutocomplete')[0];
    assertEquals('https://example.com', args[2]);
    assertTrue(args[6]);  // isOnFocus
  });

  // Like the native omnibox, a click on an empty input (e.g. on the New Tab
  // Page) queries zero-prefix suggestions even if the input is focused.
  test('FocusedClickOnEmptyInputTriggersZeroSuggest', async () => {
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: '',
      isFocused: true,
    }));
    await microtasksFinished();
    testProxy.handler.reset();

    const input = searchbox.$.input.inputElement;
    dispatchMouseEvent(input, 'mousedown', {buttons: 1});
    dispatchMouseEvent(document, 'mouseup', {buttons: 0});
    await microtasksFinished();

    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
    const args = testProxy.handler.getArgs('queryAutocomplete')[0];
    assertTrue(args[6]);  // isOnFocus
  });

  // Like the native omnibox, a click in the already focused input (e.g. after
  // a drag that selected all of the elided URL was handed off from the location
  // bar) only shows the full URL and places the caret, without querying
  // zero-prefix suggestions.
  test('FocusedClickDoesNotTriggerZeroSuggest', async () => {
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: 'example.com',
      fullUrl: 'https://example.com',
      permanentDisplayText: 'example.com',
      selection: {start: 0, end: 11},
      isFocused: true,
    }));
    await microtasksFinished();
    testProxy.handler.reset();

    const input = searchbox.$.input.inputElement;
    assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
    dispatchMouseEvent(input, 'mousedown', {buttons: 1});
    // The press leaves the elided URL selected, and the release places the
    // caret and shows the full URL.
    assertEquals('example.com', input.value);
    dispatchMouseEvent(document, 'mouseup', {buttons: 0});
    await microtasksFinished();

    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
    assertFalse(searchbox.dropdownIsVisible);
    assertEquals('https://example.com', input.value);
    assertEquals(input.selectionStart, input.selectionEnd);
  });

  test('ClickWithPartialSelectionDoesNotTriggerZeroSuggest', async () => {
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: 'https://example.com',
      fullUrl: 'https://example.com',
      isFocused: false,
    }));
    await microtasksFinished();
    testProxy.handler.reset();

    const input = searchbox.$.input.inputElement;
    dispatchMouseEvent(input, 'mousedown', {buttons: 1});
    // Simulate Blink focusing the input on mouse press.
    input.focus();
    // Simulate Blink highlighting part of the text with a drag that stays
    // within the drag threshold.
    input.setSelectionRange(8, 19);
    dispatchMouseEvent(document, 'mouseup', {buttons: 0});
    await microtasksFinished();

    // The partial selection is kept rather than replaced by select-all.
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
    assertEquals(8, input.selectionStart);
    assertEquals(19, input.selectionEnd);
  });

  // A double-click in the focused input, while the dropdown is already open
  // (e.g. from the earlier click that focused the omnibox), leaves the dropdown
  // and its matches as they are, without querying or stopping anything.
  test('DoubleClickPreservesVisibleDropdown', async () => {
    const testText = 'https://example.com';
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: testText,
      fullUrl: testText,
      isFocused: true,
      matchesUpdate: MatchesUpdate.kQueryZps,
    }));
    await microtasksFinished();
    testProxy.page.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          queryId: searchbox.activeQueryId,
          sequenceId: 1,
          input: testText,
          matches: [createSearchMatchForTesting()],
        }));
    await microtasksFinished();
    assertTrue(searchbox.dropdownIsVisible);
    testProxy.handler.reset();

    const input = searchbox.$.input.inputElement;
    dispatchMouseEvent(input, 'mousedown', {buttons: 1});
    dispatchMouseEvent(document, 'mouseup', {buttons: 0});
    dispatchMouseEvent(input, 'mousedown', {buttons: 1, detail: 2});
    dispatchMouseEvent(document, 'mouseup', {buttons: 0, detail: 2});
    await microtasksFinished();

    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
    assertEquals(0, testProxy.handler.getCallCount('stopAutocomplete'));
    assertTrue(searchbox.dropdownIsVisible);
  });

  // The second press of a double-click whose first click was handled natively
  // (e.g. it opened the dropdown from the location bar) keeps the dropdown and
  // its matches, and lets Blink select the word.
  test('NativeFirstClickDoubleClickPreservesMatches', async () => {
    const testText = 'https://example.com';
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: testText,
      fullUrl: testText,
      isFocused: true,
      matchesUpdate: MatchesUpdate.kQueryZps,
    }));
    await microtasksFinished();
    testProxy.page.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          queryId: searchbox.activeQueryId,
          sequenceId: 1,
          input: testText,
          matches: [createSearchMatchForTesting()],
        }));
    await microtasksFinished();
    assertTrue(searchbox.dropdownIsVisible);
    testProxy.handler.reset();

    const input = searchbox.$.input.inputElement;
    dispatchMouseEvent(input, 'mousedown', {buttons: 1, detail: 2});
    input.setSelectionRange(8, 15);
    dispatchMouseEvent(document, 'mouseup', {buttons: 0, detail: 2});
    await microtasksFinished();

    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
    assertEquals(0, testProxy.handler.getCallCount('stopAutocomplete'));
    assertTrue(searchbox.dropdownIsVisible);
    assertEquals(8, input.selectionStart);
    assertEquals(15, input.selectionEnd);
  });

  // A selection sync with `MatchesUpdate.kPreserveMatches` (e.g. for the second
  // click of a double-click from the location bar) keeps the visible matches,
  // stops any in-flight query, and does not issue a new one.
  test('SetInputStatePreserveMatchesKeepsDropdown', async () => {
    const testText = 'https://example.com';
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: testText,
      fullUrl: testText,
      isFocused: true,
      matchesUpdate: MatchesUpdate.kQueryZps,
    }));
    await microtasksFinished();
    const queryId = searchbox.activeQueryId;
    testProxy.page.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          queryId,
          sequenceId: 1,
          input: testText,
          matches: [createSearchMatchForTesting()],
        }));
    await microtasksFinished();
    assertTrue(searchbox.dropdownIsVisible);
    testProxy.handler.reset();

    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: testText,
      fullUrl: testText,
      isFocused: true,
      selection: {start: 8, end: 15},
      matchesUpdate: MatchesUpdate.kPreserveMatches,
    }));
    await microtasksFinished();

    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
    assertEquals(1, testProxy.handler.getCallCount('stopAutocomplete'));
    assertFalse(testProxy.handler.getArgs('stopAutocomplete')[0]);
    assertEquals(queryId, searchbox.activeQueryId);
    assertTrue(searchbox.dropdownIsVisible);
  });

  // A drag inside the WebUI omnibox, whether or not its input has DOM focus,
  // never queries zero-prefix suggestions.
  [true, false].forEach(isFocused => {
    const prefix = isFocused ? 'Focused' : 'Unfocused';

    test(`${prefix}DragDoesNotTriggerZeroSuggest`, async () => {
      callbackRouter.setInputState(createDefaultOmniboxInputState({
        text: 'https://example.com',
        fullUrl: 'https://example.com',
        isFocused,
      }));
      await microtasksFinished();
      testProxy.handler.reset();

      const input = searchbox.$.input.inputElement;
      dispatchMouseEvent(input, 'mousedown', {buttons: 1});
      input.focus();
      // Simulate Blink setting a partial drag selection.
      input.setSelectionRange(8, 19);
      // Mousemove beyond the drag threshold.
      dispatchMouseEvent(document, 'mousemove', {buttons: 1, clientX: 80});
      dispatchMouseEvent(document, 'mouseup', {buttons: 0, clientX: 80});
      await microtasksFinished();

      assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
      assertEquals(8, input.selectionStart);
      assertEquals(19, input.selectionEnd);
    });
  });

  // A double-click in the already focused input never queries zero-prefix
  // suggestions, as neither of its clicks focuses the input.
  test('FocusedDoubleClickDoesNotTriggerZeroSuggest', async () => {
    const testText = 'https://example.com';
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: testText,
      fullUrl: testText,
      isFocused: true,
    }));
    await microtasksFinished();
    testProxy.handler.reset();

    const input = searchbox.$.input.inputElement;
    dispatchMouseEvent(input, 'mousedown', {buttons: 1});
    dispatchMouseEvent(document, 'mouseup', {buttons: 0});
    dispatchMouseEvent(input, 'mousedown', {buttons: 1, detail: 2});
    input.setSelectionRange(8, 15);
    dispatchMouseEvent(document, 'mouseup', {buttons: 0, detail: 2});
    await microtasksFinished();

    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
    assertFalse(searchbox.dropdownIsVisible);
    assertEquals(8, input.selectionStart);
    assertEquals(15, input.selectionEnd);
  });

  // A double-click that focuses the input never queries zero-prefix
  // suggestions either, even though its first click does.
  test('UnfocusedDoubleClickDoesNotTriggerZeroSuggest', async () => {
    const testText = 'https://example.com';
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: testText,
      fullUrl: testText,
      isFocused: false,
    }));
    await microtasksFinished();
    testProxy.handler.reset();

    const input = searchbox.$.input.inputElement;

    // The first click looks like a single click, so it queries zero-prefix
    // suggestions on mouse release.
    dispatchMouseEvent(input, 'mousedown', {buttons: 1});
    input.focus();
    dispatchMouseEvent(document, 'mouseup', {buttons: 0});
    await microtasksFinished();
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
    const firstClickQueryId = searchbox.activeQueryId;

    // The second click turns it into a double-click, which stops that
    // query without clearing any matches.
    dispatchMouseEvent(input, 'mousedown', {buttons: 1, detail: 2});
    input.setSelectionRange(8, 15);
    dispatchMouseEvent(document, 'mouseup', {buttons: 0, detail: 2});
    await microtasksFinished();
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
    assertEquals(1, testProxy.handler.getCallCount('stopAutocomplete'));
    assertFalse(testProxy.handler.getArgs('stopAutocomplete')[0]);

    // Late results for the stopped query don't open the dropdown.
    testProxy.page.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          queryId: firstClickQueryId,
          sequenceId: 1,
          input: testText,
          matches: [createSearchMatchForTesting()],
        }));
    await microtasksFinished();
    assertFalse(searchbox.dropdownIsVisible);

    // The word selection is preserved.
    assertEquals(8, input.selectionStart);
    assertEquals(15, input.selectionEnd);
  });

  // If the first click's results arrive before the second press, the
  // double-click keeps them rather than closing the dropdown (which would
  // make it flash), but stops the query so it doesn't update them further.
  test('UnfocusedDoubleClickKeepsAlreadyShownZeroSuggest', async () => {
    const testText = 'https://example.com';
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: testText,
      fullUrl: testText,
      isFocused: false,
    }));
    await microtasksFinished();
    testProxy.handler.reset();

    const input = searchbox.$.input.inputElement;
    dispatchMouseEvent(input, 'mousedown', {buttons: 1});
    input.focus();
    dispatchMouseEvent(document, 'mouseup', {buttons: 0});
    await microtasksFinished();
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
    const firstClickQueryId = searchbox.activeQueryId;
    testProxy.page.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          queryId: firstClickQueryId,
          sequenceId: 1,
          input: testText,
          matches: [createSearchMatchForTesting()],
        }));
    await microtasksFinished();
    assertTrue(searchbox.dropdownIsVisible);

    dispatchMouseEvent(input, 'mousedown', {buttons: 1, detail: 2});
    input.setSelectionRange(8, 15);
    dispatchMouseEvent(document, 'mouseup', {buttons: 0, detail: 2});
    await microtasksFinished();
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
    assertEquals(1, testProxy.handler.getCallCount('stopAutocomplete'));
    assertFalse(testProxy.handler.getArgs('stopAutocomplete')[0]);
    assertTrue(searchbox.dropdownIsVisible);

    // Further updates for the stopped query are ignored.
    testProxy.page.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          queryId: firstClickQueryId,
          sequenceId: 2,
          input: testText,
          matches: [],
        }));
    await microtasksFinished();
    assertTrue(searchbox.dropdownIsVisible);

    assertEquals(8, input.selectionStart);
    assertEquals(15, input.selectionEnd);
  });

  test('ClearsInputTextAndNotifiesHandler', async () => {
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      sequenceNumber: 5,
      text: 'hello',
      selection: {start: 0, end: 5},
      isFocused: true,
    }));
    await microtasksFinished();
    handler.reset();

    searchbox.$.input.dispatchEvent(new CustomEvent(
        'searchbox-input-text-updated',
        {detail: {value: '   ', isComposing: false}}));
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('onInputCleared'));
    assertEquals(5, handler.getArgs('onInputCleared')[0]);
  });

  test('SetInputState_ExecutesDeferredFocusOnVisibilityChange', async () => {
    Object.defineProperty(
        document, 'visibilityState', {value: 'hidden', configurable: true});

    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: 'test',
      selection: {start: 0, end: 4},
      isFocused: true,
    }));
    await microtasksFinished();

    const input = searchbox.$.input.inputElement;
    input.blur();
    assertFalse(searchbox.shadowRoot.activeElement === searchbox.$.input);

    Object.defineProperty(
        document, 'visibilityState', {value: 'visible', configurable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await microtasksFinished();

    assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
  });

  // Verifies that when `setFocus(true)` IPC is received while
  // `document.visibilityState` is hidden, focus is deferred
  // until `visibilitychange` occurs.
  test('SetFocus_ExecutesDeferredFocusOnVisibilityChange', async () => {
    Object.defineProperty(
        document, 'visibilityState', {value: 'hidden', configurable: true});

    // Trigger dedicated `setFocus` IPC while document is hidden.
    callbackRouter.setFocus(true, false, false, /*showFullUrl=*/ false);
    await microtasksFinished();

    assertFalse(searchbox.shadowRoot.activeElement === searchbox.$.input);

    // Make visible and dispatch `visibilitychange` event.
    Object.defineProperty(
        document, 'visibilityState', {value: 'visible', configurable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await microtasksFinished();

    // Verify focus occurred.
    assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
  });

  test('SetFocus_PreservesSelectionRange', async () => {
    const testUrl = 'https://example.com';
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: testUrl,
      selection: {start: 8, end: 15},
      userInputInProgress: false,
      isFocused: true,
      matchesUpdate: MatchesUpdate.kDefault,
    }));
    await microtasksFinished();

    const input = searchbox.getInputElement().inputElement;
    assertEquals(8, input.selectionStart);
    assertEquals(15, input.selectionEnd);

    // Trigger dedicated `setFocus` IPC (e.g. refocusing or clicking top
    // chrome).
    callbackRouter.setFocus(
        true, /*queryZps=*/ false, /*selectAll=*/ false,
        /*showFullUrl=*/ false);
    await microtasksFinished();

    // Verify existing sub-selection range is preserved and not clobbered to
    // select-all.
    assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
    assertEquals(8, input.selectionStart);
    assertEquals(15, input.selectionEnd);
  });

  test('SetFocus_SelectAllWhenRequested', async () => {
    const testUrl = 'https://example.com';
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: testUrl,
      selection: {start: 8, end: 15},
      userInputInProgress: false,
      isFocused: true,
      matchesUpdate: MatchesUpdate.kDefault,
    }));
    await microtasksFinished();

    const input = searchbox.getInputElement().inputElement;
    assertEquals(8, input.selectionStart);
    assertEquals(15, input.selectionEnd);

    // Trigger dedicated `setFocus` IPC with selectAll = true.
    callbackRouter.setFocus(
        true, /*queryZps=*/ false, /*selectAll=*/ true, /*showFullUrl=*/ false);
    await microtasksFinished();

    // Verify all text is selected.
    assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
    assertEquals(0, input.selectionStart);
    assertEquals(testUrl.length, input.selectionEnd);
  });

  test('SetFocus_ShowFullUrlUnelidesThenSelectsAll', async () => {
    const displayUrl = 'example.com';
    const fullUrl = 'https://www.example.com/';
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: displayUrl,
      fullUrl: fullUrl,
      permanentDisplayText: displayUrl,
      selection: {start: 0, end: displayUrl.length},
      userInputInProgress: false,
      isFocused: true,
      matchesUpdate: MatchesUpdate.kDefault,
    }));
    await microtasksFinished();

    const input = searchbox.getInputElement().inputElement;
    assertEquals(displayUrl, input.value);

    // Trigger dedicated `setFocus` IPC with showFullUrl = true (Ctrl+L /
    // Cmd+L from outside the WebUI): the URL unelides, then all of it is
    // selected.
    callbackRouter.setFocus(
        true, /*queryZps=*/ false, /*selectAll=*/ true,
        /*showFullUrl=*/ true);
    await microtasksFinished();

    assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
    assertEquals(fullUrl, input.value);
    assertEquals(0, input.selectionStart);
    assertEquals(fullUrl.length, input.selectionEnd);
  });

  test('SetFocus_RequeriesZpsWhenSteadyStateAndDropdownClosed', async () => {
    const testUrl = 'https://example.com';
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: testUrl,
      selection: {start: 0, end: testUrl.length},
      userInputInProgress: false,
      isFocused: true,
      matchesUpdate: MatchesUpdate.kDefault,
    }));
    await microtasksFinished();

    searchbox.clearAutocompleteMatches();
    assertFalse(searchbox.dropdownIsVisible);
    testProxy.handler.resetResolver('queryAutocomplete');

    callbackRouter.setFocus(
        true, /*queryZps=*/ true, /*selectAll=*/ false, /*showFullUrl=*/ false);
    await microtasksFinished();

    const input = searchbox.getInputElement().inputElement;
    assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
    assertEquals(0, input.selectionStart);
    assertEquals(testUrl.length, input.selectionEnd);
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
    const [, , queryText, , , , isOnFocus] =
        testProxy.handler.getArgs('queryAutocomplete')[0];
    assertEquals(testUrl, queryText);
    assertTrue(isOnFocus);
  });

  test('SetFocus_DoesNotRequeryZpsWhenUserInputInProgress', async () => {
    const draftQuery = 'chrome';
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: draftQuery,
      selection: {start: 0, end: draftQuery.length},
      userInputInProgress: true,
      isFocused: true,
      matchesUpdate: MatchesUpdate.kDefault,
    }));
    await microtasksFinished();

    searchbox.dropdownIsVisible = true;
    testProxy.handler.resetResolver('queryAutocomplete');

    callbackRouter.setFocus(
        true, /*queryZps=*/ true, /*selectAll=*/ false, /*showFullUrl=*/ false);
    await microtasksFinished();

    const input = searchbox.getInputElement().inputElement;
    assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
    assertEquals(0, input.selectionStart);
    assertEquals(draftQuery.length, input.selectionEnd);
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('SetFocus_DoesNotRequeryZpsWhenDropdownAlreadyOpen', async () => {
    const testUrl = 'https://example.com';
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: testUrl,
      selection: {start: 0, end: testUrl.length},
      userInputInProgress: false,
      isFocused: true,
      matchesUpdate: MatchesUpdate.kDefault,
    }));
    await microtasksFinished();

    searchbox.dropdownIsVisible = true;
    testProxy.handler.resetResolver('queryAutocomplete');

    callbackRouter.setFocus(
        true, /*queryZps=*/ true, /*selectAll=*/ false, /*showFullUrl=*/ false);
    await microtasksFinished();

    const input = searchbox.getInputElement().inputElement;
    assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
    assertEquals(0, input.selectionStart);
    assertEquals(testUrl.length, input.selectionEnd);
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('SetFocus_DoesNotQueryZpsWhenQueryZpsIsFalse', async () => {
    const testUrl = 'https://example.com';
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: testUrl,
      selection: {start: 0, end: testUrl.length},
      userInputInProgress: false,
      isFocused: true,
      matchesUpdate: MatchesUpdate.kDefault,
    }));
    await microtasksFinished();

    searchbox.clearAutocompleteMatches();
    assertFalse(searchbox.dropdownIsVisible);
    testProxy.handler.resetResolver('queryAutocomplete');

    callbackRouter.setFocus(
        true, /*queryZps=*/ false, /*selectAll=*/ false,
        /*showFullUrl=*/ false);
    await microtasksFinished();

    const input = searchbox.getInputElement().inputElement;
    assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
    assertEquals(0, input.selectionStart);
    assertEquals(testUrl.length, input.selectionEnd);
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('RequestsAndAppliesInitialInputStateOnConnected', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const testText = 'chrome://version';

    // Mock `handler.requestInputState()` to simulate C++ responding with
    // initial state.
    handler.requestInputState = () => {
      callbackRouter.setInputState(createDefaultOmniboxInputState({
        text: testText,
        selection: {start: 0, end: testText.length},
        isFocused: true,
        permanentDisplayText: testText,
      }));
    };

    // Attach searchbox to DOM.
    const newSearchbox = document.createElement('omnibox-popup-searchbox');
    document.body.appendChild(newSearchbox);
    await microtasksFinished();

    // Verify `requestInputState()` was called once and DOM input was populated.
    assertEquals(1, handler.getCallCount('requestInputState'));
    const input = newSearchbox.$.input.inputElement;
    assertEquals(testText, input.value);
    assertEquals(0, input.selectionStart);
    assertEquals(testText.length, input.selectionEnd);
  });

  test('HandlesPastePlainText', async () => {
    const dataTransfer = new DataTransfer();
    dataTransfer.setData('text/plain', 'javascript:alert(1)\nhello world');
    const pasteEvent = new ClipboardEvent('paste', {
      clipboardData: dataTransfer,
      cancelable: true,
    });

    searchbox.$.input.dispatchEvent(pasteEvent);
    await microtasksFinished();

    assertTrue(pasteEvent.defaultPrevented);
    assertEquals('alert(1) hello world', searchbox.$.input.inputElement.value);
  });

  test('HandlesPasteBookmarkFormat', async () => {
    const dataTransfer = new DataTransfer();
    dataTransfer.setData(
        'text/x-moz-url', 'https://example.com\nExample Title');
    const pasteEvent = new ClipboardEvent('paste', {
      clipboardData: dataTransfer,
      cancelable: true,
    });

    searchbox.$.input.dispatchEvent(pasteEvent);
    await microtasksFinished();

    assertTrue(pasteEvent.defaultPrevented);
    assertEquals('https://example.com', searchbox.$.input.inputElement.value);
  });

  test('HandlesPasteAtEndScrollsToEnd', async () => {
    const longText = 'a'.repeat(200);
    const dataTransfer = new DataTransfer();
    dataTransfer.setData('text/plain', longText);
    const pasteEvent = new ClipboardEvent('paste', {
      clipboardData: dataTransfer,
      cancelable: true,
    });

    const inputElement = searchbox.$.input.inputElement;
    searchbox.$.input.dispatchEvent(pasteEvent);
    await microtasksFinished();

    assertTrue(pasteEvent.defaultPrevented);
    assertEquals(longText, inputElement.value);
    assertEquals(longText.length, inputElement.selectionStart);
    assertEquals(longText.length, inputElement.selectionEnd);
    // Verify scroll position was adjusted to show cursor at the end.
    // The browser clamps scrollLeft to max (scrollWidth - clientWidth).
    // scrollLeft can be fractional while scrollWidth and clientWidth are
    // rounded.
    assertTrue(
        Math.abs(
            inputElement.scrollLeft -
            (inputElement.scrollWidth - inputElement.clientWidth)) <= 1);
  });

  test('SetInputStateScrollsBackToTextStart', async () => {
    const text = 'abcdefghij'.repeat(40);
    const input = searchbox.$.input.inputElement;
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text,
      selection: {start: text.length, end: text.length},
      isFocused: true,
    }));
    await microtasksFinished();
    assertTrue(input.scrollLeft > 0);

    // A drag in the unfocused native omnibox from near the start past its
    // elided end. The native omnibox kept showing the start of the text.
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      sequenceNumber: 2,
      text,
      selection: {start: 5, end: text.length},
      isFocused: true,
    }));
    await microtasksFinished();

    assertEquals(5, input.selectionStart);
    assertEquals(text.length, input.selectionEnd);
    assertEquals(0, input.scrollLeft);
  });

  test('SetInputStateKeepsTextStartOnFirstFocus', async () => {
    // The first handoff from native Views focuses the input and applies the
    // range in the same update.
    const text = 'abcdefghij'.repeat(80);
    const input = searchbox.$.input.inputElement;
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text,
      selection: {start: 5, end: text.length},
      isFocused: true,
    }));
    await microtasksFinished();

    assertEquals(5, input.selectionStart);
    assertEquals(text.length, input.selectionEnd);
    assertEquals(0, input.scrollLeft);
  });

  test('SetInputStateScrollsToSelectionStart', async () => {
    const text = 'abcdefghij'.repeat(40);
    const input = searchbox.$.input.inputElement;
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text,
      selection: {start: 0, end: 0},
      isFocused: true,
    }));
    await microtasksFinished();
    assertTrue(input.scrollWidth > input.clientWidth);
    assertEquals(0, input.scrollLeft);

    // A range that starts past the first screenful of text.
    const start = text.length - 20;
    const end = text.length - 10;
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      sequenceNumber: 2,
      text,
      selection: {start, end},
      isFocused: true,
    }));
    await microtasksFinished();

    assertEquals(start, input.selectionStart);
    assertEquals(end, input.selectionEnd);
    assertTextOffsetVisible(input, start);
    // Scroll only as far as the selection start needs.
    assertTrue(input.scrollLeft < input.scrollWidth - input.clientWidth);
    // That is, just far enough to show the selection start (and the 1px caret
    // there) at the end of the visible text, which the AIM button padding
    // doesn't count towards.
    assertFalse(isVisible(searchbox.$.composeButton));
    const startRight = getTextOffsetLeft(input, start) + 1 - input.scrollLeft;
    assertTrue(
        Math.abs(getVisibleTextWidth(input) - startRight) <= 1,
        `selection start at ${startRight}px, visible width ${
            getVisibleTextWidth(input)}px`);
  });

  test('SetInputStateKeepsBackwardSelection', async () => {
    const text = 'abcdefghij'.repeat(40);
    const input = searchbox.$.input.inputElement;
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text,
      selection: {start: text.length, end: text.length},
      isFocused: true,
    }));
    await microtasksFinished();
    assertTrue(input.scrollLeft > 0);

    // A right-to-left range near the start. The caret (`end`) comes before the
    // anchor (`start`), as in a native `gfx::Range`.
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      sequenceNumber: 2,
      text,
      selection: {start: 15, end: 5},
      isFocused: true,
    }));
    await microtasksFinished();

    assertEquals(5, input.selectionStart);
    assertEquals(15, input.selectionEnd);
    assertEquals('backward', input.selectionDirection);
    assertEquals(0, input.scrollLeft);
  });

  test('SetInputStateScrollsFocusedSelectAllToTextStart', async () => {
    const text = 'abcdefghij'.repeat(80);
    const input = searchbox.$.input.inputElement;
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text,
      selection: {start: text.length, end: text.length},
      isFocused: true,
    }));
    await microtasksFinished();
    assertTrue(input.scrollLeft > 0);

    // A native select-all, which is right-to-left so the start stays in view.
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      sequenceNumber: 2,
      text,
      selection: {start: text.length, end: 0},
      isFocused: true,
    }));
    await microtasksFinished();

    assertEquals(0, input.selectionStart);
    assertEquals(text.length, input.selectionEnd);
    assertEquals(0, input.scrollLeft);
  });

  test('SetInputStateDoesNotScrollVisibleSelection', async () => {
    const text = 'abcdefghij'.repeat(40);
    const input = searchbox.$.input.inputElement;
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text,
      selection: {start: 0, end: 0},
      isFocused: true,
    }));
    await microtasksFinished();
    assertEquals(0, input.scrollLeft);

    callbackRouter.setInputState(createDefaultOmniboxInputState({
      sequenceNumber: 2,
      text,
      selection: {start: 2, end: 8},
      isFocused: true,
    }));
    await microtasksFinished();

    assertEquals(2, input.selectionStart);
    assertEquals(8, input.selectionEnd);
    assertEquals(0, input.scrollLeft);
  });

  test('HandlesCopy', async () => {
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      sequenceNumber: 7,
      text: 'hello world',
      selection: {start: 0, end: 11},
      isFocused: true,
    }));
    await microtasksFinished();
    handler.reset();

    const copyEvent = new ClipboardEvent('copy', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });

    searchbox.$.input.dispatchEvent(copyEvent);
    await microtasksFinished();

    assertTrue(copyEvent.defaultPrevented);
    assertEquals(1, handler.getCallCount('onCutOrCopy'));
    const [sequenceNumber, isCut, fullText, selection] =
        handler.getArgs('onCutOrCopy')[0];
    assertEquals(7, sequenceNumber);
    assertFalse(isCut);
    assertEquals('hello world', fullText);
    assertEquals(0, selection.start);
    assertEquals(11, selection.end);
  });

  test('HandlesCut', async () => {
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      sequenceNumber: 8,
      text: 'hello world',
      selection: {start: 0, end: 5},
      isFocused: true,
    }));
    await microtasksFinished();
    handler.reset();

    const cutEvent = new ClipboardEvent('cut', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });

    searchbox.$.input.dispatchEvent(cutEvent);
    await microtasksFinished();

    assertTrue(cutEvent.defaultPrevented);
    assertEquals(1, handler.getCallCount('onCutOrCopy'));
    const [sequenceNumber, isCut, fullText, selection] =
        handler.getArgs('onCutOrCopy')[0];
    assertEquals(8, sequenceNumber);
    assertTrue(isCut);
    assertEquals('hello world', fullText);
    assertEquals(0, selection.start);
    assertEquals(5, selection.end);

    // Verify local input element was updated to remaining text (" world")
    // and caret moved to start of cut position (0, 0).
    const input = searchbox.$.input.inputElement;
    assertEquals(' world', input.value);
    assertEquals(0, input.selectionStart);
    assertEquals(0, input.selectionEnd);

    // Verify Undo restores the cut text and re-selects it.
    input.dispatchEvent(new InputEvent('beforeinput', {
      inputType: 'historyUndo',
      bubbles: true,
      cancelable: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals('hello world', input.value);
    assertEquals(0, input.selectionStart);
    assertEquals(5, input.selectionEnd);

    // Verify Redo re-cuts the text.
    input.dispatchEvent(new InputEvent('beforeinput', {
      inputType: 'historyRedo',
      bubbles: true,
      cancelable: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals(' world', input.value);
    assertEquals(0, input.selectionStart);
    assertEquals(0, input.selectionEnd);
  });

  test('CutUndoRedoAfterPaste', async () => {
    // Start with empty input.
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      sequenceNumber: 9,
      text: '',
      selection: {start: 0, end: 0},
      isFocused: true,
    }));
    await microtasksFinished();
    handler.reset();

    const input = searchbox.$.input.inputElement;

    // Paste "hello world".
    const dataTransfer = new DataTransfer();
    dataTransfer.setData('text/plain', 'hello world');
    const pasteEvent = new ClipboardEvent('paste', {
      clipboardData: dataTransfer,
      cancelable: true,
      bubbles: true,
      composed: true,
    });
    searchbox.$.input.dispatchEvent(pasteEvent);
    await microtasksFinished();
    assertEquals('hello world', input.value);

    // Select "world" (indices 6 to 11).
    input.setSelectionRange(6, 11);

    // Cut "world".
    const cutEvent = new ClipboardEvent('cut', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    searchbox.$.input.dispatchEvent(cutEvent);
    await microtasksFinished();
    assertEquals('hello ', input.value);
    assertEquals(6, input.selectionStart);
    assertEquals(6, input.selectionEnd);

    // Undo should restore the cut text ("world") instead of undoing paste
    // (which would have cleared the input).
    input.dispatchEvent(new InputEvent('beforeinput', {
      inputType: 'historyUndo',
      bubbles: true,
      cancelable: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals('hello world', input.value);
    assertEquals(6, input.selectionStart);
    assertEquals(11, input.selectionEnd);

    // Undoing again should undo the paste, returning to empty string.
    input.dispatchEvent(new InputEvent('beforeinput', {
      inputType: 'historyUndo',
      bubbles: true,
      cancelable: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals('', input.value);

    // Redo should restore pasted text "hello world".
    input.dispatchEvent(new InputEvent('beforeinput', {
      inputType: 'historyRedo',
      bubbles: true,
      cancelable: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals('hello world', input.value);

    // Redoing again should re-apply the cut ("hello ").
    input.dispatchEvent(new InputEvent('beforeinput', {
      inputType: 'historyRedo',
      bubbles: true,
      cancelable: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals('hello ', input.value);
    assertEquals(6, input.selectionStart);
    assertEquals(6, input.selectionEnd);
  });

  // Focuses the empty input (as on the NTP), shows zero-prefix suggestions and
  // arrows down to preview the first one, `fillIntoEdit`, in the input.
  async function previewFirstZeroSuggestMatch(fillIntoEdit: string) {
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      isFocused: true,
      matchesUpdate: MatchesUpdate.kQueryZps,
    }));
    await microtasksFinished();
    testProxy.page.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          queryId: searchbox.activeQueryId,
          input: '',
          matches: [
            createSearchMatchForTesting({fillIntoEdit}),
            createSearchMatchForTesting({fillIntoEdit: 'news'}),
          ],
        }));
    await microtasksFinished();
    assertTrue(searchbox.dropdownIsVisible);

    searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      key: 'ArrowDown',
    }));
    await microtasksFinished();
    assertEquals(fillIntoEdit, searchbox.$.input.inputElement.value);
  }

  test('CutPartOfPreviewedMatch', async () => {
    await previewFirstZeroSuggestMatch('weather today');
    handler.reset();

    // Select "today" and cut it.
    const input = searchbox.$.input.inputElement;
    input.setSelectionRange(8, 13);
    const cutEvent = new ClipboardEvent('cut', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    searchbox.$.input.dispatchEvent(cutEvent);
    await microtasksFinished();

    assertTrue(cutEvent.defaultPrevented);
    assertEquals(1, handler.getCallCount('onCutOrCopy'));
    const [, isCut, fullText, selection] = handler.getArgs('onCutOrCopy')[0];
    assertTrue(isCut);
    assertEquals('weather today', fullText);
    assertEquals(8, selection.start);
    assertEquals(13, selection.end);

    // Only the selected text is cut, rather than the whole input.
    assertEquals('weather ', input.value);
    assertEquals(8, input.selectionStart);
    assertEquals(8, input.selectionEnd);

    // Undo restores the cut text and re-selects it.
    input.dispatchEvent(new InputEvent('beforeinput', {
      inputType: 'historyUndo',
      bubbles: true,
      cancelable: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals('weather today', input.value);
    assertEquals(8, input.selectionStart);
    assertEquals(13, input.selectionEnd);
  });

  test('PasteOverPartOfPreviewedMatch', async () => {
    await previewFirstZeroSuggestMatch('weather today');

    // Select "today" and paste over it.
    const input = searchbox.$.input.inputElement;
    input.setSelectionRange(8, 13);
    const dataTransfer = new DataTransfer();
    dataTransfer.setData('text/plain', 'tomorrow');
    const pasteEvent = new ClipboardEvent('paste', {
      clipboardData: dataTransfer,
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    searchbox.$.input.dispatchEvent(pasteEvent);
    await microtasksFinished();

    assertTrue(pasteEvent.defaultPrevented);
    assertEquals('weather tomorrow', input.value);
    assertEquals(16, input.selectionStart);
    assertEquals(16, input.selectionEnd);

    // Undo restores the replaced text and re-selects it.
    input.dispatchEvent(new InputEvent('beforeinput', {
      inputType: 'historyUndo',
      bubbles: true,
      cancelable: true,
      composed: true,
    }));
    await microtasksFinished();
    assertEquals('weather today', input.value);
    assertEquals(8, input.selectionStart);
    assertEquals(13, input.selectionEnd);
  });

  test('UndoAfterCutOfPreviewedMatchStepsBackThroughTyping', async () => {
    const input = searchbox.$.input.inputElement;
    input.focus();
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      isFocused: true,
    }));
    await microtasksFinished();

    // Type "hello", one character at a time.
    for (let i = 1; i <= 'hello'.length; i++) {
      input.value = 'hello'.substring(0, i);
      input.setSelectionRange(i, i);
      input.dispatchEvent(
          new InputEvent('input', {bubbles: true, composed: true}));
      await microtasksFinished();
    }

    // Arrow down to preview the non-default match "hello world".
    testProxy.page.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          queryId: searchbox.activeQueryId,
          input: 'hello',
          matches: [
            createSearchMatchForTesting(
                {allowedToBeDefaultMatch: true, fillIntoEdit: 'hello'}),
            createSearchMatchForTesting({fillIntoEdit: 'hello world'}),
          ],
        }));
    await microtasksFinished();
    searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      key: 'ArrowDown',
    }));
    await microtasksFinished();
    assertEquals('hello world', input.value);

    // Select "hello " and cut it.
    input.setSelectionRange(0, 6);
    searchbox.$.input.dispatchEvent(new ClipboardEvent(
        'cut', {bubbles: true, cancelable: true, composed: true}));
    await microtasksFinished();
    assertEquals('world', input.value);

    // Each undo steps back one edit: the cut, then the previewed match, then
    // the typing, back to the initial empty text.
    const undo = async () => {
      input.dispatchEvent(new InputEvent('beforeinput', {
        inputType: 'historyUndo',
        bubbles: true,
        cancelable: true,
        composed: true,
      }));
      await microtasksFinished();
    };
    await undo();
    assertEquals('hello world', input.value);
    assertEquals(0, input.selectionStart);
    assertEquals(6, input.selectionEnd);
    await undo();
    assertEquals('hello', input.value);
    await undo();
    assertEquals('', input.value);
    await undo();
    assertEquals('', input.value);
  });

  test('WordDeletionUndoRedo', async () => {
    searchbox.$.input.focus();
    const inputEl = searchbox.$.input.inputElement;

    // Baseline text: "hello world" with caret at index 11.
    inputEl.value = 'hello world';
    inputEl.setSelectionRange(11, 11);
    searchbox.$.input.fire(
        'searchbox-input-text-updated',
        {value: 'hello world', isComposing: false});
    await microtasksFinished();

    // User performs word deletion (Alt/Ctrl + Backspace): text becomes "hello
    // ", cursor moves to 6.
    inputEl.value = 'hello ';
    inputEl.selectionStart = 6;
    inputEl.selectionEnd = 6;
    searchbox.$.input.fire(
        'searchbox-input-text-updated', {value: 'hello ', isComposing: false});
    await microtasksFinished();
    assertEquals('hello ', inputEl.value);

    // Trigger Undo. The deleted word "world" should be restored AND selected
    // (6..11).
    inputEl.dispatchEvent(new InputEvent('beforeinput', {
      inputType: 'historyUndo',
      bubbles: true,
      composed: true,
      cancelable: true,
    }));
    await microtasksFinished();
    assertEquals('hello world', inputEl.value);
    assertEquals(6, inputEl.selectionStart);
    assertEquals(11, inputEl.selectionEnd);
  });

  test('StripSchemasUnsafeForPaste', () => {
    const testCases: Array<{input: string, expected: string}> = [
      // Safe query.
      {input: ' \x01 ', expected: ' \x01 '},
      // Safe URL.
      {
        input: 'http://www.google.com?q=javascript:alert(0)',
        expected: 'http://www.google.com?q=javascript:alert(0)',
      },
      // Safe query.
      {input: 'JavaScript', expected: 'JavaScript'},
      // Unsafe JS URL.
      {input: 'javaScript:', expected: ''},
      // Unsafe JS URL.
      {input: ' javaScript: ', expected: ''},
      // Unsafe JS URL.
      {
        input: 'javAscript:Javascript:javascript',
        expected: 'javascript',
      },
      // Unsafe JS URL.
      {input: 'javAscript:alert(1)', expected: 'alert(1)'},
      // Single strip unsafe.
      {
        input: 'javAscript:javascript:alert(2)',
        expected: 'alert(2)',
      },
      // Single strip unsafe.
      {
        input: 'jaVascript:\njavaScript:\x01 alert(3) \x01',
        expected: 'alert(3) \x01',
      },
      // Leading control chars unsafe.
      {
        input:
            '\x01\x02\x03\x04\x05\x06\x07\x08\x09\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\u2009JavaScript:alert(4)',
        expected: 'alert(4)',
      },
      // Embedded control characters unsafe.
      {
        input: '\x01\x02javascript:\x03\x04JavaScript:alert(5)',
        expected: 'alert(5)',
      },
    ];

    for (const testCase of testCases) {
      assertEquals(testCase.expected, stripJavascriptSchemas(testCase.input));
    }
  });

  test('SanitizeTextForPaste', () => {
    const testCases: Array<{input: string, expected: string}> = [
      // No whitespace: leave unchanged.
      {input: '', expected: ''},
      {input: 'a', expected: 'a'},
      {input: 'abc', expected: 'abc'},

      // Leading/trailing whitespace: remove.
      {input: ' abc', expected: 'abc'},
      {input: '  \n  abc', expected: 'abc'},
      {input: 'abc ', expected: 'abc'},
      {input: 'abc\t \t', expected: 'abc'},
      {input: '\nabc\n', expected: 'abc'},

      // All whitespace: Convert to single space.
      {input: ' ', expected: ' '},
      {input: '\n', expected: ' '},
      {input: '   ', expected: ' '},
      {input: '\n\n\n', expected: ' '},
      {input: ' \n\t', expected: ' '},

      // Broken URL has newlines stripped.
      {
        input: 'http://www.chromium.org/developers/testing/chromium-\n' +
            'build-infrastructure/tour-of-the-chromium-buildbot',
        expected: 'http://www.chromium.org/developers/testing/' +
            'chromium-build-infrastructure/tour-of-the-chromium-buildbot',
      },

      // Multi-line address is converted to a single-line address.
      {
        input: '1600 Amphitheatre Parkway\nMountain View, CA',
        expected: '1600 Amphitheatre Parkway Mountain View, CA',
      },

      // Line-breaking the JavaScript scheme with no other whitespace results in
      // a dangerous URL that is sanitized by dropping the scheme.
      {input: 'java\r\nscript:alert(0)', expected: 'alert(0)'},

      // Line-breaking the JavaScript scheme with whitespace elsewhere in the
      // string results in a safe string with a space replacing the line break.
      {input: 'java\r\nscript: alert(0)', expected: 'java script: alert(0)'},

      // Unusual URL with multiple internal spaces is preserved as-is.
      {input: 'http://foo.com/a.  b', expected: 'http://foo.com/a.  b'},

      // URL with unicode whitespace is also preserved as-is.
      {input: 'http://foo.com/a\u3000b', expected: 'http://foo.com/a\u3000b'},
    ];

    for (const testCase of testCases) {
      assertEquals(testCase.expected, sanitizeTextForPaste(testCase.input));
    }
  });

  test('SetsPopupSelectionOnMatchIndexChange', async () => {
    // Initial state: nothing selected.
    assertEquals(-1, searchbox.selectedMatchIndex);

    // Populate autocomplete result so match index 2 is valid and not out of
    // bounds.
    searchbox.activeQueryId = 0;
    searchbox.lastQueriedInput = '';
    testProxy.page.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          queryId: 0,
          input: '',
          matches: [
            createSearchMatchForTesting(),
            createSearchMatchForTesting(),
            createSearchMatchForTesting(),
          ],
        }));
    await microtasksFinished();

    testProxy.handler.reset();

    // Change selection to a valid match index.
    searchbox.selectedMatchIndex = 2;
    await microtasksFinished();

    // Verify handler was notified with correct selection.
    assertEquals(1, testProxy.handler.getCallCount('setPopupSelection'));
    let args = testProxy.handler.getArgs('setPopupSelection');
    let selection = args[args.length - 1];
    assertEquals(2, selection.line);
    assertEquals(SelectionLineState.kNormal, selection.state);
    assertEquals(0, selection.actionIndex);

    // Reset selection to -1.
    searchbox.selectedMatchIndex = -1;
    await microtasksFinished();

    // Verify handler was notified with kDefaultSelection (line -1).
    assertEquals(2, testProxy.handler.getCallCount('setPopupSelection'));
    args = testProxy.handler.getArgs('setPopupSelection');
    selection = args[args.length - 1];
    assertEquals(-1, selection.line);
    assertEquals(SelectionLineState.kNormal, selection.state);
    assertEquals(0, selection.actionIndex);
  });

 test('InputWrapperFocusout', async () => {
    // Set input value to match results.
    searchbox.getInputElement().inputElement.value = 'hello';
    searchbox.lastQueriedInput = 'hello';
    searchbox.activeQueryId = 0;

    // Populate results to make dropdown visible.
    testProxy.page.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'hello',
          matches: [
            createSearchMatchForTesting({
              allowedToBeDefaultMatch: true,
              fillIntoEdit: 'hello world',
            }),
          ],
        }));
    await microtasksFinished();
    assertTrue(searchbox.dropdownIsVisible);

    // Focus stays inside wrapper.
    searchbox.$.inputWrapper.dispatchEvent(new FocusEvent('focusout', {
      relatedTarget: searchbox.$.matches,
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    // Verify matches are not cleared and dropdown remains visible.
    assertTrue(searchbox.dropdownIsVisible);
    assertEquals(0, handler.getCallCount('revert'));

    // Focus goes outside wrapper.
    searchbox.$.inputWrapper.dispatchEvent(new FocusEvent('focusout', {
      relatedTarget: document.body,
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    // Verify matches are cleared, dropdown is hidden, and selection is reset to
    // (0, 0).
    assertFalse(searchbox.dropdownIsVisible);
    assertEquals(0, handler.getCallCount('revert'));
    assertEquals(0, searchbox.getInputElement().inputElement.selectionStart);
    assertEquals(0, searchbox.getInputElement().inputElement.selectionEnd);
  });

 test('ComputePlaceholderText_OnTabSwitchAndStateReset', async () => {
   // Initial NTP tab state (empty input, empty `permanentDisplayText`,
   // unfocused).
   callbackRouter.setInputState(createDefaultOmniboxInputState());
   await microtasksFinished();
   await searchbox.$.input.updateComplete;

   // Placeholder must always be empty.
   assertEquals('', searchbox.$.input.inputElement.placeholder);

   // Switch to regular URL tab (permanentDisplayText set, focused).
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     sequenceNumber: 2,
     text: 'chrome://version',
     selection: {start: 16, end: 16},
     fullUrl: 'chrome://version',
     isFocused: true,
     permanentDisplayText: 'chrome://version',
   }));
   await microtasksFinished();
   await searchbox.$.input.updateComplete;

   // Placeholder must always be empty.
   assertEquals('', searchbox.$.input.inputElement.placeholder);
 });

 test('TabSwitchInputStateIsolationAndReset', async () => {
   // Simulate Tab 1 (NTP) state with active user draft.
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     sequenceNumber: 10,
     text: 'user search query',
     selection: {start: 17, end: 17},
     userInputInProgress: true,
     isFocused: true,
   }));
   await microtasksFinished();

   assertEquals('user search query', searchbox.$.input.inputElement.value);
   assertEquals('user search query', searchbox.lastQueriedInput);
   assertEquals(17, searchbox.$.input.inputElement.selectionStart);
   assertEquals(17, searchbox.$.input.inputElement.selectionEnd);

   // Tab switch to Tab 2 (non-NTP) with permanent URL.
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     sequenceNumber: 11,
     text: 'https://chromium.org',
     selection: {start: 20, end: 20},
     fullUrl: 'https://chromium.org',
     isFocused: true,
     permanentDisplayText: 'https://chromium.org',
   }));
   await microtasksFinished();

   assertEquals('https://chromium.org', searchbox.$.input.inputElement.value);
   assertEquals('https://chromium.org', searchbox.lastQueriedInput);
   assertEquals(20, searchbox.$.input.inputElement.selectionStart);
   assertEquals(20, searchbox.$.input.inputElement.selectionEnd);
 });

 test('KeepsDropdownOpenOnBackgroundTabNavigation', async () => {
   // Set some input text to query autocomplete.
   const mockInput = searchbox.$.input;
   mockInput.inputElement.value = 'test';
   mockInput.inputElement.dispatchEvent(new Event('test', {bubbles: true}));

   // Simulate autocomplete results.
   searchbox.lastQueriedInput = 'test';
   searchbox.onAutocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: searchbox.activeQueryId,
     input: 'test',
     matches: [
       createSearchMatchForTesting({
         allowedToBeDefaultMatch: true,
         inlineAutocompletion: 'ing',
         fillIntoEdit: 'testing',
       }),
       createSearchMatchForTesting(),
     ],
   }));
   await microtasksFinished();
   assertTrue(searchbox.dropdownIsVisible);
   assertEquals('testing', searchbox.getInputElement().getInputValue());
   assertEquals('test', searchbox.getInputElement().lastInput()?.text);
   assertEquals('ing', searchbox.getInputElement().lastInput()?.inline);

   // Simulate `Enter` with Alt + Shift keys (background tab).
   searchbox.navigateToMatch(
       0,
       new KeyboardEvent(
           'keydown', {key: 'Enter', altKey: true, shiftKey: true}));
   await microtasksFinished();
   assertTrue(searchbox.dropdownIsVisible);
   assertEquals('testing', searchbox.getInputElement().getInputValue());
   assertEquals('test', searchbox.getInputElement().lastInput()?.text);
   assertEquals('ing', searchbox.getInputElement().lastInput()?.inline);

   // Simulate `Enter` with Meta key and without Shift key (background tab).
   searchbox.navigateToMatch(
       0,
       new KeyboardEvent(
           'keydown', {key: 'Enter', metaKey: true, shiftKey: false}));
   await microtasksFinished();
   assertTrue(searchbox.dropdownIsVisible);
   assertEquals('testing', searchbox.getInputElement().getInputValue());
   assertEquals('test', searchbox.getInputElement().lastInput()?.text);
   assertEquals('ing', searchbox.getInputElement().lastInput()?.inline);

   // Simulate a normal Enter key (foreground tab).
   searchbox.navigateToMatch(0, new KeyboardEvent('keydown', {key: 'Enter'}));
   await microtasksFinished();

   // Dropdown should now be closed.
   assertFalse(searchbox.dropdownIsVisible);
 });

 test('IgnoreOutOfBoundsMatchIndexChange', async () => {
   assertEquals(-1, searchbox.selectedMatchIndex);

   searchbox.activeQueryId = 0;
   searchbox.lastQueriedInput = '';
   testProxy.page.autocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: 0,
     input: '',
     matches: [
       createSearchMatchForTesting(),
     ],
   }));
   await microtasksFinished();

   testProxy.handler.reset();

   // Set an out-of-bounds index.
   searchbox.selectedMatchIndex = 5;
   await microtasksFinished();

   // Verify handler was not notified because index is out of bounds.
   assertEquals(0, testProxy.handler.getCallCount('setPopupSelection'));
 });

 test('InputWrapperFocusout_NullRelatedTarget', async () => {
   searchbox.activeQueryId = 0;
   searchbox.lastQueriedInput = 'hello';
   testProxy.page.autocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: 0,
     input: 'hello',
     matches: [createSearchMatchForTesting()],
   }));
   await microtasksFinished();
   assertTrue(searchbox.dropdownIsVisible);

   // DOM focusout with `relatedTarget: null` is ignored because window-level
   // focus loss is managed via Mojo IPC (SetFocus).
   searchbox.$.inputWrapper.dispatchEvent(new FocusEvent('focusout', {
     relatedTarget: null,
     bubbles: true,
     composed: true,
   }));
   await microtasksFinished();
   assertTrue(searchbox.dropdownIsVisible);

   // Receiving `setFocus(false)` via Mojo IPC triggers focus-loss cleanup.
   callbackRouter.setFocus(
       false, /*queryZps=*/ false, /*selectAll=*/ false,
       /*showFullUrl=*/ false);
   await microtasksFinished();
   assertFalse(searchbox.dropdownIsVisible);
 });

 test('EscapeStagedUnwinding', async () => {
   for (const virtualFocus of [false, true]) {
     searchbox.virtualFocusEnabled = virtualFocus;
     handler.reset();
     testProxy.handler.reset();

     // Stage 1 (`kRevertTemporaryText`):
     searchbox.getInputElement().inputElement.value = 'a';
     searchbox.lastQueriedInput = 'a';
     searchbox.activeQueryId = 0;
     testProxy.page.autocompleteResultChanged(
         createAutocompleteResultForTesting({
           input: 'a',
           matches: [
             createSearchMatchForTesting({
               allowedToBeDefaultMatch: true,
               fillIntoEdit: 'a',
               inlineAutocompletion: '',
             }),
             createSearchMatchForTesting({
               allowedToBeDefaultMatch: false,
               fillIntoEdit: 'suggestion-1',
             }),
           ],
         }));
     await microtasksFinished();
     assertTrue(searchbox.dropdownIsVisible);

     if (virtualFocus) {
       searchbox.setSelection({
         line: 1,
         state: SelectionLineState.kNormal,
         actionIndex: 0,
       });
     } else {
       searchbox.selectedMatchIndex = 1;
     }
     searchbox.getInputElement().inputElement.value = 'suggestion-1';
     await microtasksFinished();

     await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
       key: 'Escape',
       cancelable: true,
     }));
     await microtasksFinished();

     assertEquals('a', searchbox.getInputElement().inputElement.value);
     if (virtualFocus) {
       assertEquals(0, searchbox.selection.line);
       assertEquals(SelectionLineState.kNormal, searchbox.selection.state);
       assertEquals(0, searchbox.selection.actionIndex);
     } else {
       assertEquals(0, searchbox.selectedMatchIndex);
     }
     assertTrue(searchbox.dropdownIsVisible);
     assertEquals(1, handler.getCallCount('logEscapeAction'));
     assertEquals(
         OmniboxEscapeAction.kRevertTemporaryText,
         handler.getArgs('logEscapeAction')[0]);

     handler.reset();
     testProxy.handler.reset();

     // Stage 2 (`kClosePopup`):
     await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
       key: 'Escape',
       cancelable: true,
     }));
     await microtasksFinished();

     assertFalse(searchbox.dropdownIsVisible);
     assertEquals(1, testProxy.handler.getCallCount('stopAutocomplete'));
     assertTrue(testProxy.handler.getArgs('stopAutocomplete')[0]);
     assertEquals(1, handler.getCallCount('logEscapeAction'));
     assertEquals(
         OmniboxEscapeAction.kClosePopup,
         handler.getArgs('logEscapeAction')[0]);

     handler.reset();
     testProxy.handler.reset();

     // Stage 3 (`kClearUserInput` - elided URL where showFullUrl is false):
     const permanentDisplayText = 'example.com';
     const fullUrl = 'https://example.com/';
     callbackRouter.setInputState(createDefaultOmniboxInputState({
       sequenceNumber: 5,
       text: 'dirty input',
       selection: {start: 1, end: 1},
       userInputInProgress: true,
       fullUrl: fullUrl,
       isFocused: true,
       permanentDisplayText: permanentDisplayText,
     }));
     await microtasksFinished();

     await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
       key: 'Escape',
       cancelable: true,
     }));
     await microtasksFinished();

     assertEquals(
         permanentDisplayText, searchbox.getInputElement().inputElement.value);
     assertEquals(0, searchbox.getInputElement().inputElement.selectionStart);
     assertEquals(
         permanentDisplayText.length,
         searchbox.getInputElement().inputElement.selectionEnd);
     assertEquals(1, handler.getCallCount('revert'));
     assertEquals(5, handler.getArgs('revert')[0]);
     assertEquals(1, handler.getCallCount('logEscapeAction'));
     assertEquals(
         OmniboxEscapeAction.kClearUserInput,
         handler.getArgs('logEscapeAction')[0]);

     handler.reset();
     testProxy.handler.reset();

     // Stage 3 (`kClearUserInput` - unelided URL where showFullUrl is true):
     callbackRouter.setInputState(createDefaultOmniboxInputState({
       sequenceNumber: 6,
       text: 'dirty input',
       selection: {start: 1, end: 1},
       userInputInProgress: true,
       fullUrl: fullUrl,
       isFocused: true,
       permanentDisplayText: permanentDisplayText,
       showFullUrl: true,
     }));
     await microtasksFinished();

     await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
       key: 'Escape',
       cancelable: true,
     }));
     await microtasksFinished();

     assertEquals(
         'example.com', searchbox.getInputElement().inputElement.value);
     assertEquals(0, searchbox.getInputElement().inputElement.selectionStart);
     assertEquals(
         permanentDisplayText.length,
         searchbox.getInputElement().inputElement.selectionEnd);
     assertEquals(1, handler.getCallCount('revert'));
     assertEquals(6, handler.getArgs('revert')[0]);
     assertEquals(1, handler.getCallCount('logEscapeAction'));
     assertEquals(
         OmniboxEscapeAction.kClearUserInput,
         handler.getArgs('logEscapeAction')[0]);

     handler.reset();
     testProxy.handler.reset();

     // Stage 4 (`kBlur`):
     await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
       key: 'Escape',
       cancelable: true,
     }));
     await microtasksFinished();

     assertEquals(1, handler.getCallCount('revert'));
     assertEquals(6, handler.getArgs('revert')[0]);
     assertEquals(1, handler.getCallCount('closeUI'));
     assertEquals(1, handler.getCallCount('logEscapeAction'));
     assertEquals(
         OmniboxEscapeAction.kBlur, handler.getArgs('logEscapeAction')[0]);
   }
 });

 test('EscapeStagedUnwinding_ElidedUrlSelectionRevertedOnClose', async () => {
   // Clicking the omnibox selects the elided URL (`showFullUrl: false`,
   // `userInputInProgress: false`) and opens the zero-prefix suggestions
   // dropdown. Pressing Escape twice (1st closes dropdown, 2nd blurs and closes
   // popup) must revert/clear the selection.
   const permanentDisplayText = 'example.com';
   const fullUrl = 'https://example.com/';
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     sequenceNumber: 12,
     text: permanentDisplayText,
     selection: {start: 0, end: permanentDisplayText.length},
     userInputInProgress: false,
     fullUrl: fullUrl,
     isFocused: true,
     permanentDisplayText: permanentDisplayText,
     showFullUrl: false,
   }));
   await microtasksFinished();

   searchbox.activeQueryId = 0;
   searchbox.lastQueriedInput = '';
   searchbox.onAutocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: 0,
     input: '',
     matches: [createSearchMatchForTesting()],
   }));
   await microtasksFinished();

   assertTrue(searchbox.dropdownIsVisible);
   assertEquals(0, searchbox.getInputElement().inputElement.selectionStart);
   assertEquals(
       permanentDisplayText.length,
       searchbox.getInputElement().inputElement.selectionEnd);

   // 1st Escape (Stage 2: `kClosePopup`) closes dropdown; selection remains.
   await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
     key: 'Escape',
     cancelable: true,
   }));
   await microtasksFinished();

   assertFalse(searchbox.dropdownIsVisible);
   assertEquals(0, handler.getCallCount('revert'));
   assertEquals(0, handler.getCallCount('closeUI'));
   assertEquals(1, handler.getCallCount('logEscapeAction'));
   assertEquals(
       OmniboxEscapeAction.kClosePopup, handler.getArgs('logEscapeAction')[0]);

   handler.reset();
   testProxy.handler.reset();

   // 2nd Escape (Stage 4: `kBlur`) reverts selection and closes UI.
   await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
     key: 'Escape',
     cancelable: true,
   }));
   await microtasksFinished();

   assertEquals(1, handler.getCallCount('revert'));
   assertEquals(12, handler.getArgs('revert')[0]);
   assertEquals(1, handler.getCallCount('closeUI'));
   assertEquals(1, handler.getCallCount('logEscapeAction'));
   assertEquals(
       OmniboxEscapeAction.kBlur, handler.getArgs('logEscapeAction')[0]);
 });

 test('EscapeStagedUnwinding_UnelidedUrlClosesDirectly', async () => {
   // When the URL is unelided without user edits (`userInputInProgress: false`,
   // `showFullUrl: true`) and the dropdown is closed, pressing Escape should
   // skip Stage 3 (`kClearUserInput`) and directly execute Stage 4 (`kBlur`).
   const permanentDisplayText = 'example.com';
   const fullUrl = 'https://example.com/';
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     sequenceNumber: 9,
     text: permanentDisplayText,
     selection: {start: 0, end: permanentDisplayText.length},
     userInputInProgress: false,
     fullUrl: fullUrl,
     isFocused: true,
     permanentDisplayText: permanentDisplayText,
     showFullUrl: true,
   }));
   await microtasksFinished();

   assertEquals(fullUrl, searchbox.getInputElement().inputElement.value);

   await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
     key: 'Escape',
     cancelable: true,
   }));
   await microtasksFinished();

   assertEquals(1, handler.getCallCount('revert'));
   assertEquals(9, handler.getArgs('revert')[0]);
   assertEquals(1, handler.getCallCount('closeUI'));
   assertEquals(1, handler.getCallCount('logEscapeAction'));
   assertEquals(
       OmniboxEscapeAction.kBlur, handler.getArgs('logEscapeAction')[0]);
 });

 test('EscapeStagedUnwinding_ClearedInputNonEmptyUrl', async () => {
   // Input was manually cleared ('') on a page with a non-empty permanent URL.
   // ESC should restore the permanent URL ('example.com') without closing UI.
   const permanentDisplayText = 'example.com';
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     sequenceNumber: 8,
     userInputInProgress: true,
     fullUrl: 'https://example.com/',
     isFocused: true,
     permanentDisplayText: permanentDisplayText,
   }));
   await microtasksFinished();

   await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
     key: 'Escape',
     cancelable: true,
   }));
   await microtasksFinished();

   assertEquals(
       permanentDisplayText, searchbox.getInputElement().inputElement.value);
   assertEquals(0, handler.getCallCount('closeUI'));
   assertEquals(1, handler.getCallCount('revert'));
   assertEquals(1, handler.getCallCount('logEscapeAction'));
   assertEquals(
       OmniboxEscapeAction.kClearUserInput,
       handler.getArgs('logEscapeAction')[0]);
 });

 test('EscapeStagedUnwinding_EmptyPermanentUrl', async () => {
   // Scenario 1: Typed text on NTP ('dirty NTP input').
   // 1st ESC reverts text to empty string without closing UI.
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     sequenceNumber: 6,
     text: 'dirty NTP input',
     selection: {start: 1, end: 1},
     userInputInProgress: true,
     isFocused: true,
   }));
   await microtasksFinished();

   await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
     key: 'Escape',
     cancelable: true,
   }));
   await microtasksFinished();

   assertEquals('', searchbox.getInputElement().inputElement.value);
   assertEquals(1, handler.getCallCount('revert'));
   assertEquals(6, handler.getArgs('revert')[0]);
   assertEquals(0, handler.getCallCount('closeUI'));
   assertEquals(1, handler.getCallCount('logEscapeAction'));
   assertEquals(
       OmniboxEscapeAction.kClearUserInput,
       handler.getArgs('logEscapeAction')[0]);

   handler.reset();
   testProxy.handler.reset();

   // Scenario 2: Input was already empty ('') on NTP after clearing.
   // 1st ESC clears user input AND closes UI immediately (avoiding empty ->
   // empty no-op).
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     sequenceNumber: 7,
     userInputInProgress: true,
     isFocused: true,
   }));
   await microtasksFinished();

   await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
     key: 'Escape',
     cancelable: true,
   }));
   await microtasksFinished();

   assertEquals('', searchbox.getInputElement().inputElement.value);
   assertEquals(1, handler.getCallCount('revert'));
   assertEquals(7, handler.getArgs('revert')[0]);
   assertEquals(1, handler.getCallCount('closeUI'));
   assertEquals(1, handler.getCallCount('logEscapeAction'));
   assertEquals(
       OmniboxEscapeAction.kClearUserInput,
       handler.getArgs('logEscapeAction')[0]);
 });

 test('EscapeStagedUnwinding_KeywordMode', async () => {
   // Scenario: User is in keyword mode with a typed query on a webpage.
   // 1st ESC closes dropdown; keyword mode remains active.
   // 2nd ESC clears user input, exits keyword mode, and restores permanent URL.
   // 3rd ESC blurs and closes omnibox.
   const permanentDisplayText = 'example.com';
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     sequenceNumber: 10,
     text: 'search query',
     selection: {start: 12, end: 12},
     userInputInProgress: true,
     fullUrl: 'https://example.com/',
     isFocused: true,
     permanentDisplayText: permanentDisplayText,
     keywordModel: {
       type: KeywordType.kInKeyword,
       keyword: 'youtube.com',
       displayText: 'Search YouTube',
       iconPath: '',
       placeholder: '',
     },
   }));
   await microtasksFinished();

   searchbox.activeQueryId = 0;
   searchbox.lastQueriedInput = 'search query';
   searchbox.getInputElement().inputElement.value = 'search query';
   searchbox.onAutocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: 0,
     input: 'search query',
     matches: [
       createSearchMatchForTesting({
         allowedToBeDefaultMatch: true,
         fillIntoEdit: 'search query',
         keywordModel: createMatchKeywordModelForTesting({
           type: KeywordType.kInKeyword,
           keyword: 'youtube.com',
         }),
       }),
     ],
   }));
   await microtasksFinished();

   assertTrue(searchbox.dropdownIsVisible);
   assertTrue(searchbox.keywordModeManager.isInKeywordMode);
   assertEquals('youtube.com', searchbox.inputKeywordModel?.keyword);

   // 1st Escape (Stage 2: kClosePopup)
   await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
     key: 'Escape',
     cancelable: true,
   }));
   await microtasksFinished();

   assertFalse(searchbox.dropdownIsVisible);
   assertTrue(searchbox.keywordModeManager.isInKeywordMode);
   assertEquals('youtube.com', searchbox.inputKeywordModel?.keyword);
   assertEquals(1, handler.getCallCount('logEscapeAction'));
   assertEquals(
       OmniboxEscapeAction.kClosePopup, handler.getArgs('logEscapeAction')[0]);

   handler.reset();
   testProxy.handler.reset();

   // 2nd Escape (Stage 3: kClearUserInput) - exits keyword mode & restores URL.
   await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
     key: 'Escape',
     cancelable: true,
   }));
   await microtasksFinished();

   assertFalse(searchbox.keywordModeManager.isInKeywordMode);
   assertEquals(null, searchbox.inputKeywordModel);
   assertEquals(
       permanentDisplayText, searchbox.getInputElement().inputElement.value);
   assertEquals(0, searchbox.getInputElement().inputElement.selectionStart);
   assertEquals(
       permanentDisplayText.length,
       searchbox.getInputElement().inputElement.selectionEnd);
   assertEquals(1, handler.getCallCount('revert'));
   assertEquals(10, handler.getArgs('revert')[0]);
   assertEquals(0, handler.getCallCount('closeUI'));
   assertEquals(1, handler.getCallCount('logEscapeAction'));
   assertEquals(
       OmniboxEscapeAction.kClearUserInput,
       handler.getArgs('logEscapeAction')[0]);

   handler.reset();
   testProxy.handler.reset();

   // 3rd Escape (Stage 4: kBlur) - reverts selection and closes UI.
   await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
     key: 'Escape',
     cancelable: true,
   }));
   await microtasksFinished();

   assertEquals(1, handler.getCallCount('revert'));
   assertEquals(10, handler.getArgs('revert')[0]);
   assertEquals(1, handler.getCallCount('closeUI'));
   assertEquals(1, handler.getCallCount('logEscapeAction'));
   assertEquals(
       OmniboxEscapeAction.kBlur, handler.getArgs('logEscapeAction')[0]);
 });

 test('EscapeStagedUnwinding_KeywordMode_EmptyPermanentUrl', async () => {
   // Scenario: User enters keyword mode on NTP (permanentDisplayText is empty)
   // with no query typed, and keyword suggestions dropdown is open.
   // 1st ESC closes dropdown; keyword mode remains active.
   // 2nd ESC clears keyword mode and keeps focus in Omnibox.
   // 3rd ESC blurs and closes omnibox.
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     sequenceNumber: 11,
     text: '',
     userInputInProgress: true,
     isFocused: true,
     permanentDisplayText: '',
     keywordModel: {
       type: KeywordType.kInKeyword,
       keyword: 'youtube.com',
       displayText: 'Search YouTube',
       iconPath: '',
       placeholder: '',
     },
   }));
   await microtasksFinished();

   searchbox.activeQueryId = 0;
   searchbox.lastQueriedInput = '';
   searchbox.getInputElement().inputElement.value = '';
   searchbox.onAutocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: 0,
     input: '',
     matches: [
       createSearchMatchForTesting({
         allowedToBeDefaultMatch: true,
         fillIntoEdit: 'youtube.com',
         keywordModel: createMatchKeywordModelForTesting({
           type: KeywordType.kInKeyword,
           keyword: 'youtube.com',
         }),
       }),
     ],
   }));
   await microtasksFinished();

   assertTrue(searchbox.dropdownIsVisible);
   assertTrue(searchbox.keywordModeManager.isInKeywordMode);
   assertEquals('youtube.com', searchbox.inputKeywordModel?.keyword);

   // 1st Escape (Stage 2: kClosePopup) - closes dropdown, preserves keyword
   // mode.
   await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
     key: 'Escape',
     cancelable: true,
   }));
   await microtasksFinished();

   assertFalse(searchbox.dropdownIsVisible);
   assertTrue(searchbox.keywordModeManager.isInKeywordMode);
   assertEquals('youtube.com', searchbox.inputKeywordModel?.keyword);
   assertEquals(1, handler.getCallCount('logEscapeAction'));
   assertEquals(
       OmniboxEscapeAction.kClosePopup, handler.getArgs('logEscapeAction')[0]);

   handler.reset();
   testProxy.handler.reset();

   // 2nd Escape (Stage 3: kClearUserInput) - exits keyword mode, stays open.
   await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
     key: 'Escape',
     cancelable: true,
   }));
   await microtasksFinished();

   assertFalse(searchbox.keywordModeManager.isInKeywordMode);
   assertEquals(null, searchbox.inputKeywordModel);
   assertEquals('', searchbox.getInputElement().inputElement.value);
   assertEquals(1, handler.getCallCount('revert'));
   assertEquals(11, handler.getArgs('revert')[0]);
   assertEquals(0, handler.getCallCount('closeUI'));
   assertEquals(1, handler.getCallCount('logEscapeAction'));
   assertEquals(
       OmniboxEscapeAction.kClearUserInput,
       handler.getArgs('logEscapeAction')[0]);

   handler.reset();
   testProxy.handler.reset();

   // 3rd Escape (Stage 4: kBlur) - reverts selection and closes UI.
   await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
     key: 'Escape',
     cancelable: true,
   }));
   await microtasksFinished();

   assertEquals(1, handler.getCallCount('revert'));
   assertEquals(11, handler.getArgs('revert')[0]);
   assertEquals(1, handler.getCallCount('closeUI'));
   assertEquals(1, handler.getCallCount('logEscapeAction'));
   assertEquals(
       OmniboxEscapeAction.kBlur, handler.getArgs('logEscapeAction')[0]);
 });

 test('EscapeIgnoredDuringIMEComposition', async () => {
   searchbox.lastQueriedInput = 'a';
   searchbox.activeQueryId = 0;
   testProxy.page.autocompleteResultChanged(createAutocompleteResultForTesting({
     input: 'a',
     matches: [
       createSearchMatchForTesting({
         allowedToBeDefaultMatch: true,
         fillIntoEdit: 'autocomplete.com',
         inlineAutocompletion: 'utocomplete.com',
       }),
     ],
   }));
   await microtasksFinished();
   assertTrue(searchbox.dropdownIsVisible);

   // Press Escape while IME composition is active (isComposing: true).
   await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
     key: 'Escape',
     isComposing: true,
     cancelable: true,
   }));
   await microtasksFinished();

   // Escape should be ignored so the OS IME engine can handle it.
   // Popup should remain open and logEscapeAction should not be called.
   assertTrue(searchbox.dropdownIsVisible);
   assertEquals(0, handler.getCallCount('logEscapeAction'));
 });

 test('SecondarySideShows', async () => {
   // Ensure `canShowSecondarySide` is set to true.
   searchbox.canShowSecondarySide = true;
   await microtasksFinished();

   const matches = [
     createSearchMatchForTesting({suggestionGroupId: 1}),
     createSearchMatchForTesting({suggestionGroupId: 100}),
   ];
   const suggestionGroupsMap = {
     1: {
       header: 'Primary',
       renderType: RenderType.kDefaultVertical,
       sideType: SideType.kDefaultPrimary,
     },
     100: {
       header: 'Secondary',
       renderType: RenderType.kDefaultVertical,
       sideType: SideType.kSecondary,
     },
   };

   searchbox.onAutocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: searchbox.activeQueryId,
     sequenceId: 1001,
     input: 'test',
     matches: matches,
     suggestionGroupsMap: suggestionGroupsMap,
   }));
   await microtasksFinished();

   assertTrue(searchbox.hasSecondarySide);

   // Verify `secondary-side` element is rendered and visible.
   assertTrue(isVisible($$(searchbox.$.matches, '.secondary-side')));

   // Verify secondary side is hidden when `canShowSecondarySide` is false.
   searchbox.canShowSecondarySide = false;
   await microtasksFinished();
   assertFalse(isVisible($$(searchbox.$.matches, '.secondary-side')));
 });

 test('OpensAimPopupWhenComposeButtonClicked', async () => {
   searchbox.onAutocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: searchbox.activeQueryId,
     sequenceId: 456,
     input: 'test',
     matches: [createSearchMatchForTesting()],
   }));
   await microtasksFinished();
   assertTrue(searchbox.dropdownIsVisible);

   const composeButton = searchbox.$.composeButton;
   assertTrue(!!composeButton);
   composeButton.$.composeButton.dispatchEvent(new MouseEvent('click', {
     bubbles: true,
     composed: true,
     detail: 1,
   }));
   await microtasksFinished();

   assertFalse(searchbox.dropdownIsVisible);
   const viaKeyboard = await handler.whenCalled('openAimPopup');
   assertFalse(viaKeyboard);
 });

 test('OpensAimPopupWhenComposeButtonKeyboardActivated', async () => {
   searchbox.onAutocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: searchbox.activeQueryId,
     sequenceId: 789,
     input: 'test',
     matches: [createSearchMatchForTesting()],
   }));
   await microtasksFinished();
   assertTrue(searchbox.dropdownIsVisible);

   const composeButton = searchbox.$.composeButton;
   assertTrue(!!composeButton);
   composeButton.$.composeButton.dispatchEvent(new MouseEvent('click', {
     bubbles: true,
     composed: true,
     detail: 0,
   }));
   await microtasksFinished();

   assertFalse(searchbox.dropdownIsVisible);
   const viaKeyboard = await handler.whenCalled('openAimPopup');
   assertTrue(viaKeyboard);
 });

 test('OnPaste', async () => {
   const input = searchbox.getInputElement().inputElement;
   input.focus();

   const dataTransfer = new DataTransfer();
   dataTransfer.setData('text/plain', 'https://example.com');
   const pasteEvent = new ClipboardEvent('paste', {
     clipboardData: dataTransfer,
     bubbles: true,
     cancelable: true,
     composed: true,
   });

   input.dispatchEvent(pasteEvent);
   await microtasksFinished();

   assertEquals('https://example.com', input.value);
   assertEquals(1, handler.getCallCount('onPaste'));
   const [pastedText, selection, sequenceNum] = handler.getArgs('onPaste')[0];
   assertEquals('https://example.com', pastedText);
   assertEquals(19, selection.start);
   assertEquals(19, selection.end);
   assertEquals(0, sequenceNum);

   assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
   const [
     _queryId,
     _tabId,
     queryText,
     preventInline,
     _cursorPos,
     _inventory,
     isOnFocus,
   ] = testProxy.handler.getArgs('queryAutocomplete')[0];
   assertEquals('https://example.com', queryText);
   assertTrue(preventInline);
   assertFalse(isOnFocus);
 });

 test('UndoRedoInsertions', async () => {
   const inputEl = searchbox.getInputElement().inputElement;
   inputEl.focus();
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: '',
     isFocused: true,
   }));
   await microtasksFinished();

   // Type 't', 'e', 's', 't' sequentially as individual character input events.
   let currentText = '';
   for (const char of 'test') {
     currentText += char;
     inputEl.value = currentText;
     const cursorPos = currentText.length;
     inputEl.setSelectionRange(cursorPos, cursorPos);
     inputEl.dispatchEvent(
         new InputEvent('input', {bubbles: true, composed: true}));
     await microtasksFinished();
   }

   assertEquals('test', inputEl.value);

   // Trigger Undo shortcut (Ctrl+Z / Cmd+Z)
   inputEl.dispatchEvent(new KeyboardEvent('keydown', {
     key: 'z',
     ctrlKey: !isMac,
     metaKey: isMac,
     bubbles: true,
     composed: true,
   }));
   await microtasksFinished();

   // A single Undo should revert all merged typed characters back to ""
   assertEquals('', inputEl.value);

   // Trigger Redo shortcut (Ctrl+Shift+Z / Cmd+Shift+Z)
   inputEl.dispatchEvent(new KeyboardEvent('keydown', {
     key: 'z',
     shiftKey: true,
     ctrlKey: !isMac,
     metaKey: isMac,
     bubbles: true,
     composed: true,
   }));
   await microtasksFinished();

   // A single Redo should restore "test"
   assertEquals('test', inputEl.value);
 });

 const focusLocationShortcuts:
     Array<{name: string, eventInit: KeyboardEventInit}> = [
       {
         name: 'CmdCtrlL',
         eventInit: {key: 'l', ctrlKey: !isMac, metaKey: isMac},
       },
       ...(!isMac ? [{
         name: 'AltD',
         eventInit: {key: 'd', code: 'KeyD', altKey: true},
       }] :
                    []),
     ];

 focusLocationShortcuts.forEach(({name, eventInit}) => {
   test(`${name}_SelectsTextWhenUserInputInProgress`, async () => {
     const draftQuery = 'chrome query';
     callbackRouter.setInputState(createDefaultOmniboxInputState({
       text: draftQuery,
       userInputInProgress: true,
       isFocused: true,
       matchesUpdate: MatchesUpdate.kDefault,
     }));
     await microtasksFinished();

     const inputEl = searchbox.getInputElement().inputElement;
     inputEl.setSelectionRange(2, 2);
     testProxy.handler.resetResolver('queryAutocomplete');

     inputEl.dispatchEvent(new KeyboardEvent('keydown', {
       ...eventInit,
       bubbles: true,
       composed: true,
     }));
     await microtasksFinished();

     assertEquals(0, inputEl.selectionStart);
     assertEquals(draftQuery.length, inputEl.selectionEnd);
     assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
   });

   test(
       `${name}_SelectsTextAndQueriesZpsWhenUserInputNotInProgress`,
       async () => {
         const testUrl = 'https://example.com';
         callbackRouter.setInputState(createDefaultOmniboxInputState({
           text: testUrl,
           userInputInProgress: false,
           isFocused: true,
           matchesUpdate: MatchesUpdate.kDefault,
         }));
         await microtasksFinished();

         searchbox.clearAutocompleteMatches();
         assertFalse(searchbox.dropdownIsVisible);
         testProxy.handler.resetResolver('queryAutocomplete');

         const inputEl = searchbox.getInputElement().inputElement;
         inputEl.setSelectionRange(3, 3);

         inputEl.dispatchEvent(new KeyboardEvent('keydown', {
           ...eventInit,
           bubbles: true,
           composed: true,
         }));
         await microtasksFinished();

         assertEquals(0, inputEl.selectionStart);
         assertEquals(testUrl.length, inputEl.selectionEnd);
         assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
         const [, , queryText, , , , isOnFocus] =
             testProxy.handler.getArgs('queryAutocomplete')[0];
         assertEquals(testUrl, queryText);
         assertTrue(isOnFocus);
       });

   test(`${name}_DoesNotRequeryZpsWhenDropdownAlreadyOpen`, async () => {
     const testUrl = 'https://example.com';
     callbackRouter.setInputState(createDefaultOmniboxInputState({
       text: testUrl,
       userInputInProgress: false,
       isFocused: true,
       matchesUpdate: MatchesUpdate.kDefault,
     }));
     await microtasksFinished();

     searchbox.dropdownIsVisible = true;
     testProxy.handler.resetResolver('queryAutocomplete');

     const inputEl = searchbox.getInputElement().inputElement;
     inputEl.setSelectionRange(3, 3);

     inputEl.dispatchEvent(new KeyboardEvent('keydown', {
       ...eventInit,
       bubbles: true,
       composed: true,
     }));
     await microtasksFinished();

     assertEquals(0, inputEl.selectionStart);
     assertEquals(testUrl.length, inputEl.selectionEnd);
     assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
   });

   test(`${name}_ClearsKeywordModeAndSelectsRestoredText`, async () => {
     callbackRouter.setInputState(createDefaultOmniboxInputState({
       text: '',
       userInputInProgress: true,
       isFocused: true,
       matchesUpdate: MatchesUpdate.kDefault,
     }));
     await microtasksFinished();

     // Enter keyword mode with some text.
     searchbox.keywordModeManager.enter(
         'google.com', 'Search Google', KeywordModeEntryMethod.TAB,
         'Search Google');
     searchbox.getInputElement().setInputText('flowers');
     assertTrue(searchbox.keywordModeManager.isInKeywordMode);

     testProxy.handler.resetResolver('queryAutocomplete');
     const inputEl = searchbox.getInputElement().inputElement;

     // Dispatch focus location shortcut (Cmd/Ctrl + L or Alt + D).
     inputEl.dispatchEvent(new KeyboardEvent('keydown', {
       ...eventInit,
       bubbles: true,
       composed: true,
     }));
     await microtasksFinished();

     // Keyword mode should be cleared, text restored with keyword prefix, and
     // all text selected.
     assertFalse(searchbox.keywordModeManager.isInKeywordMode);
     assertEquals('google.com flowers', inputEl.value);
     assertEquals(0, inputEl.selectionStart);
     assertEquals('google.com flowers'.length, inputEl.selectionEnd);
     assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
     const [, , queryText] = testProxy.handler.getArgs('queryAutocomplete')[0];
     assertEquals('google.com flowers', queryText);
   });

   test(
       `${name}_UnelidesSelectsFullUrlAndUpdatesUrlEmphasisWhenFocused`,
       async () => {
         const displayUrl = 'example.com';
         const fullUrl = 'https://example.com/';
         const elidedEmphasis: UrlEmphasis = {
           deemphasisMode: UrlDeemphasisMode.kAllButHost,
           textIsUrl: true,
           schemeRange: {start: 0, end: 0},
           hostRange: {start: 0, end: 11},
           schemeStrikeThrough: false,
           schemeDangerous: false,
         };
         const unelidedEmphasis: UrlEmphasis = {
           deemphasisMode: UrlDeemphasisMode.kAllButHost,
           textIsUrl: true,
           schemeRange: {start: 0, end: 5},
           hostRange: {start: 8, end: 19},
           schemeStrikeThrough: false,
           schemeDangerous: false,
         };
         callbackRouter.setInputState(createDefaultOmniboxInputState({
           text: displayUrl,
           fullUrl: fullUrl,
           permanentDisplayText: displayUrl,
           urlEmphasis: elidedEmphasis,
           unelidedUrlEmphasis: unelidedEmphasis,
           selection: {start: 0, end: displayUrl.length},
           userInputInProgress: false,
           isFocused: true,
           matchesUpdate: MatchesUpdate.kQueryZps,
         }));
         await microtasksFinished();

         const inputEl = searchbox.getInputElement().inputElement;
         assertEquals(displayUrl, inputEl.value);

         inputEl.dispatchEvent(new KeyboardEvent('keydown', {
           ...eventInit,
           bubbles: true,
           composed: true,
         }));
         await microtasksFinished();

         assertEquals(fullUrl, inputEl.value);
         assertDeepEquals(unelidedEmphasis, searchbox.$.input.urlEmphasis);
         assertEquals(0, inputEl.selectionStart);
         assertEquals(fullUrl.length, inputEl.selectionEnd);
       });
 });

 test('FocusSearch_ClearsPermanentUrlWhenUserInputNotInProgress', async () => {
   testProxy.page.setAvailableKeywordModels(
       [{
         type: KeywordType.kChip,
         keyword: 'google.com',
         displayText: 'Search Google',
         iconPath: '',
         placeholder: 'Search Google',
       }],
       'google.com');
   await microtasksFinished();

   const testUrl = 'https://example.com';
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: testUrl,
     userInputInProgress: false,
     isFocused: true,
     matchesUpdate: MatchesUpdate.kDefault,
   }));
   await microtasksFinished();

   testProxy.handler.resetResolver('queryAutocomplete');

   callbackRouter.focusSearchWithDefaultSearchEngineKeywordMode();
   await microtasksFinished();

   const inputEl = searchbox.getInputElement().inputElement;
   assertEquals('', inputEl.value);
   assertEquals('google.com', searchbox.inputKeywordModel?.keyword);
   assertEquals('Search Google', searchbox.inputKeywordModel?.displayText);
   assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
   const [, , queryText, , , , isOnFocus, keyword] =
       testProxy.handler.getArgs('queryAutocomplete')[0];
   assertEquals('', queryText);
   assertFalse(isOnFocus);
   assertEquals('google.com', keyword);
 });

 test('FocusSearch_UndoRevertsToEmptyNotFullUrl', async () => {
   testProxy.page.setAvailableKeywordModels(
       [{
         type: KeywordType.kChip,
         keyword: 'google.com',
         displayText: 'Search Google',
         iconPath: '',
         placeholder: 'Search Google',
       }],
       'google.com');
   await microtasksFinished();

   const permanentText = 'example.com';
   const fullUrl = 'https://www.example.com/';
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: permanentText,
     fullUrl: fullUrl,
     permanentDisplayText: permanentText,
     selection: {start: 0, end: fullUrl.length},
     userInputInProgress: false,
     isFocused: true,
     showFullUrl: true,
     matchesUpdate: MatchesUpdate.kQueryZps,
   }));
   await microtasksFinished();

   callbackRouter.focusSearchWithDefaultSearchEngineKeywordMode();
   await microtasksFinished();

   const inputEl = searchbox.getInputElement().inputElement;
   assertEquals('', inputEl.value);
   assertTrue(searchbox.keywordModeManager.isInKeywordMode);

   // Type 'hello' in keyword mode.
   inputEl.value = 'hello';
   inputEl.setSelectionRange(5, 5);
   inputEl.dispatchEvent(
       new InputEvent('input', {bubbles: true, composed: true}));
   await microtasksFinished();
   assertEquals('hello', inputEl.value);

   // Undo should revert to empty input (''), not the full URL.
   inputEl.dispatchEvent(new KeyboardEvent('keydown', {
     key: 'z',
     ctrlKey: !isMac,
     metaKey: isMac,
     bubbles: true,
     composed: true,
   }));
   await microtasksFinished();

   assertEquals('', inputEl.value);
   assertTrue(searchbox.keywordModeManager.isInKeywordMode);

   // Further undo should be a no-op and keep empty input ('').
   inputEl.dispatchEvent(new KeyboardEvent('keydown', {
     key: 'z',
     ctrlKey: !isMac,
     metaKey: isMac,
     bubbles: true,
     composed: true,
   }));
   await microtasksFinished();

   assertEquals('', inputEl.value);
   assertTrue(searchbox.keywordModeManager.isInKeywordMode);
 });

 test('FocusSearch_SelectsDraftTextWhenUserInputInProgress', async () => {
   testProxy.page.setAvailableKeywordModels(
       [{
         type: KeywordType.kChip,
         keyword: 'google.com',
         displayText: 'Search Google',
         iconPath: '',
         placeholder: 'Search Google',
       }],
       'google.com');
   await microtasksFinished();

   const draftQuery = 'chrome query';
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: draftQuery,
     userInputInProgress: true,
     isFocused: true,
     matchesUpdate: MatchesUpdate.kDefault,
   }));
   await microtasksFinished();

   const inputEl = searchbox.getInputElement().inputElement;
   inputEl.setSelectionRange(2, 2);
   testProxy.handler.resetResolver('queryAutocomplete');

   inputEl.dispatchEvent(new KeyboardEvent('keydown', {
     key: isMac ? 'f' : 'k',
     code: isMac ? 'KeyF' : 'KeyK',
     ctrlKey: !isMac,
     metaKey: isMac,
     altKey: isMac,
     bubbles: true,
     composed: true,
   }));
   await microtasksFinished();

   assertEquals(draftQuery, inputEl.value);
   assertEquals(0, inputEl.selectionStart);
   assertEquals(draftQuery.length, inputEl.selectionEnd);
   assertEquals('google.com', searchbox.inputKeywordModel?.keyword);
   assertEquals('Search Google', searchbox.inputKeywordModel?.displayText);
   assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
   const [, , queryText, , , , isOnFocus, keyword] =
       testProxy.handler.getArgs('queryAutocomplete')[0];
   assertEquals(draftQuery, queryText);
   assertFalse(isOnFocus);
   assertEquals('google.com', keyword);
 });

 test('BackspaceToEmptyInKeywordModeKeepsDropdownOpen', async () => {
   // Register the default search engine's keyword so Ctrl+K can resolve it.
   testProxy.page.setAvailableKeywordModels(
       [{
         type: KeywordType.kChip,
         keyword: 'google.com',
         displayText: 'Search Google',
         iconPath: '',
         placeholder: 'Search Google',
       }],
       'google.com');
   await microtasksFinished();

   // Start on a page with its URL shown and no user input.
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: 'https://example.com',
     userInputInProgress: false,
     isFocused: true,
   }));
   await microtasksFinished();

   // Ctrl+K enters keyword mode for the default search engine.
   callbackRouter.focusSearchWithDefaultSearchEngineKeywordMode();
   await microtasksFinished();
   assertTrue(searchbox.keywordModeManager.isInKeywordMode);

   // Type a character and deliver results so the dropdown is open.
   const inputEl = searchbox.getInputElement().inputElement;
   inputEl.value = 'a';
   inputEl.setSelectionRange(1, 1);
   inputEl.dispatchEvent(
       new InputEvent('input', {bubbles: true, composed: true}));
   await microtasksFinished();

   testProxy.page.autocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: searchbox.activeQueryId,
     input: 'a',
     matches: [
       createSearchMatchForTesting({
         allowedToBeDefaultMatch: true,
         fillIntoEdit: 'a',
         keywordModel: createMatchKeywordModelForTesting({
           type: KeywordType.kInKeyword,
           keyword: 'google.com',
         }),
       }),
     ],
   }));
   await microtasksFinished();
   assertTrue(searchbox.dropdownIsVisible);

   handler.reset();
   testProxy.handler.reset();

   // Backspace until the input is empty.
   inputEl.value = '';
   inputEl.setSelectionRange(0, 0);
   inputEl.dispatchEvent(new InputEvent(
       'input',
       {inputType: 'deleteContentBackward', bubbles: true, composed: true}));
   await microtasksFinished();

   // Keyword mode is preserved and the dropdown stays open. The empty input
   // is not treated as cleared; a keyword-scoped query is issued instead.
   assertTrue(searchbox.keywordModeManager.isInKeywordMode);
   assertTrue(searchbox.dropdownIsVisible);
   assertEquals(0, testProxy.handler.getCallCount('stopAutocomplete'));
   assertEquals(0, handler.getCallCount('onInputCleared'));
   assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
   const [, , queryText, , , , , queryKeyword] =
       testProxy.handler.getArgs('queryAutocomplete')[0];
   assertEquals('', queryText);
   assertEquals('google.com', queryKeyword);
 });

 test('CutAndUndoToEmptyInKeywordModeKeepsDropdownOpen', async () => {
   // Same invariant as `BackspaceToEmptyInKeywordModeKeepsDropdownOpen`, but
   // for the cut and undo/redo edit paths, which have their own "is the input
   // now empty?" checks.
   testProxy.page.setAvailableKeywordModels(
       [{
         type: KeywordType.kChip,
         keyword: 'google.com',
         displayText: 'Search Google',
         iconPath: '',
         placeholder: 'Search Google',
       }],
       'google.com');
   await microtasksFinished();

   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: '',
     userInputInProgress: false,
     isFocused: true,
   }));
   await microtasksFinished();

   callbackRouter.focusSearchWithDefaultSearchEngineKeywordMode();
   await microtasksFinished();
   assertTrue(searchbox.keywordModeManager.isInKeywordMode);

   // Type a query and deliver results so the dropdown is open.
   const inputEl = searchbox.getInputElement().inputElement;
   inputEl.value = 'cats';
   inputEl.setSelectionRange(4, 4);
   inputEl.dispatchEvent(
       new InputEvent('input', {bubbles: true, composed: true}));
   await microtasksFinished();

   testProxy.page.autocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: searchbox.activeQueryId,
     input: 'cats',
     matches: [
       createSearchMatchForTesting({
         allowedToBeDefaultMatch: true,
         fillIntoEdit: 'cats',
         keywordModel: createMatchKeywordModelForTesting({
           type: KeywordType.kInKeyword,
           keyword: 'google.com',
         }),
       }),
     ],
   }));
   await microtasksFinished();
   assertTrue(searchbox.dropdownIsVisible);

   // Cut all text (Ctrl+A, Ctrl+X).
   handler.reset();
   testProxy.handler.reset();
   inputEl.setSelectionRange(0, 4);
   inputEl.dispatchEvent(new ClipboardEvent('cut', {
     clipboardData: new DataTransfer(),
     bubbles: true,
     composed: true,
     cancelable: true,
   }));
   await microtasksFinished();

   assertEquals('', inputEl.value);
   assertTrue(searchbox.keywordModeManager.isInKeywordMode);
   assertTrue(searchbox.dropdownIsVisible);
   assertEquals(0, testProxy.handler.getCallCount('stopAutocomplete'));
   assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
   const [, , cutQueryText, , , , , cutQueryKeyword] =
       testProxy.handler.getArgs('queryAutocomplete')[0];
   assertEquals('', cutQueryText);
   assertEquals('google.com', cutQueryKeyword);

   // Undo restores the text; redo empties it again. Redo-to-empty must also
   // keep the dropdown open.
   handler.reset();
   testProxy.handler.reset();
   inputEl.dispatchEvent(new KeyboardEvent('keydown', {
     key: 'z',
     ctrlKey: !isMac,
     metaKey: isMac,
     bubbles: true,
     composed: true,
   }));
   await microtasksFinished();
   assertEquals('cats', inputEl.value);
   assertTrue(searchbox.keywordModeManager.isInKeywordMode);

   handler.reset();
   testProxy.handler.reset();
   inputEl.dispatchEvent(new KeyboardEvent('keydown', {
     key: 'z',
     shiftKey: true,
     ctrlKey: !isMac,
     metaKey: isMac,
     bubbles: true,
     composed: true,
   }));
   await microtasksFinished();

   assertEquals('', inputEl.value);
   assertTrue(searchbox.keywordModeManager.isInKeywordMode);
   assertTrue(searchbox.dropdownIsVisible);
   assertEquals(0, testProxy.handler.getCallCount('stopAutocomplete'));
   assertEquals(0, handler.getCallCount('onInputCleared'));
   assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
   const [, , redoQueryText, , , , , redoQueryKeyword] =
       testProxy.handler.getArgs('queryAutocomplete')[0];
   assertEquals('', redoQueryText);
   assertEquals('google.com', redoQueryKeyword);
 });

 test('UndoRedoBeforeInput', async () => {
   const inputEl = searchbox.getInputElement().inputElement;
   inputEl.focus();
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: '',
     isFocused: true,
   }));
   await microtasksFinished();

   // Type 'hello'
   inputEl.value = 'hello';
   inputEl.setSelectionRange(5, 5);
   inputEl.dispatchEvent(
       new InputEvent('input', {bubbles: true, composed: true}));
   await microtasksFinished();
   assertEquals('hello', inputEl.value);

   // Dispatch beforeinput with inputType 'historyUndo' (simulates Context Menu
   // / Edit Menu Undo)
   inputEl.dispatchEvent(new InputEvent('beforeinput', {
     inputType: 'historyUndo',
     bubbles: true,
     composed: true,
     cancelable: true,
   }));
   await microtasksFinished();
   assertEquals('', inputEl.value);

   // Dispatch beforeinput with inputType 'historyRedo' (simulates Context Menu
   // / Edit Menu Redo)
   inputEl.dispatchEvent(new InputEvent('beforeinput', {
     inputType: 'historyRedo',
     bubbles: true,
     composed: true,
     cancelable: true,
   }));
   await microtasksFinished();
   assertEquals('hello', inputEl.value);
 });

 test('BlockInsertionNonMergeable', async () => {
   const inputEl = searchbox.getInputElement().inputElement;
   inputEl.focus();
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: '',
     isFocused: true,
   }));
   await microtasksFinished();

   // Insert a multi-character block 'hello' (length > 1 -> non-mergeable).
   inputEl.value = 'hello';
   inputEl.setSelectionRange(5, 5);
   inputEl.dispatchEvent(
       new InputEvent('input', {bubbles: true, composed: true}));
   await microtasksFinished();
   assertEquals('hello', inputEl.value);

   // Type a single character '!' (length 1 -> mergeable).
   inputEl.value = 'hello!';
   inputEl.setSelectionRange(6, 6);
   searchbox.$.input.fire(
       'searchbox-input-text-updated', {value: 'hello!', isComposing: false});
   await microtasksFinished();
   assertEquals('hello!', inputEl.value);

   // First Undo reverts '!' back to 'hello' (because 'hello' was
   // non-mergeable).
   inputEl.dispatchEvent(new InputEvent('beforeinput', {
     inputType: 'historyUndo',
     bubbles: true,
     composed: true,
     cancelable: true,
   }));
   await microtasksFinished();
   assertEquals('hello', inputEl.value);

   // Second Undo reverts 'hello' back to ''.
   inputEl.dispatchEvent(new InputEvent('beforeinput', {
     inputType: 'historyUndo',
     bubbles: true,
     composed: true,
     cancelable: true,
   }));
   await microtasksFinished();
   assertEquals('', inputEl.value);
 });

 test('SelectionReplacementUndoRedo', async () => {
   searchbox.$.input.focus();
   const inputEl = searchbox.$.input.inputElement;

   // Step 1: User types 'world'.
   inputEl.value = 'world';
   inputEl.setSelectionRange(5, 5);
   searchbox.$.input.fire(
       'searchbox-input-text-updated', {value: 'world', isComposing: false});
   await microtasksFinished();

   // Step 2: User selects all text 'world' (0..5).
   inputEl.setSelectionRange(0, 5);
   document.dispatchEvent(new Event('selectionchange'));
   await microtasksFinished();

   // Step 3: User replaces selected 'world' with 'universe'.
   inputEl.value = 'universe';
   inputEl.setSelectionRange(8, 8);
   searchbox.$.input.fire(
       'searchbox-input-text-updated', {value: 'universe', isComposing: false});
   await microtasksFinished();
   assertEquals('universe', inputEl.value);

   inputEl.dispatchEvent(new InputEvent('beforeinput', {
     inputType: 'historyUndo',
     bubbles: true,
     composed: true,
     cancelable: true,
   }));
   await microtasksFinished();
   assertEquals('world', inputEl.value);
   assertEquals(0, inputEl.selectionStart);
   assertEquals(5, inputEl.selectionEnd);

   inputEl.dispatchEvent(new InputEvent('beforeinput', {
     inputType: 'historyRedo',
     bubbles: true,
     composed: true,
     cancelable: true,
   }));
   await microtasksFinished();
   assertEquals('universe', inputEl.value);
 });

 test(
     'arrow up to default URL match restores typed text without https scheme',
     async () => {
       // Setup initial input state.
       searchbox.getInputElement().inputElement.value = 'hello';
       searchbox.lastQueriedInput = 'hello';
       searchbox.activeQueryId = 0;

       // Simulate autocomplete results.
       const matches = [
         createSearchMatchForTesting({
           allowedToBeDefaultMatch: true,
           isSearchType: false,
           fillIntoEdit: 'https://helloworld.com',
           inlineAutocompletion: 'world.com',
         }),
         createSearchMatchForTesting({
           fillIntoEdit: 'hello second',
         }),
       ];
       testProxy.page.autocompleteResultChanged(
           createAutocompleteResultForTesting({
             queryId: searchbox.activeQueryId,
             input: 'hello',
             matches: matches,
           }));
       await microtasksFinished();

       assertTrue(searchbox.dropdownIsVisible);
       assertEquals(
           'helloworld.com', searchbox.getInputElement().inputElement.value);

       // Arrow down to second match.
       searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
         bubbles: true,
         cancelable: true,
         key: 'ArrowDown',
       }));
       await microtasksFinished();

       assertEquals(1, searchbox.selectedMatchIndex);
       assertEquals(
           'hello second', searchbox.getInputElement().inputElement.value);

       // Arrow up back to default match.
       searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
         bubbles: true,
         cancelable: true,
         key: 'ArrowUp',
       }));
       await microtasksFinished();

       assertEquals(0, searchbox.selectedMatchIndex);
       assertEquals(
           'helloworld.com', searchbox.getInputElement().inputElement.value);

       // Check that internal state matches original query, meaning slicing
       // worked properly and https:// wasn't prepended.
       assertEquals('hello', searchbox.getInputElement().lastInput()?.text);
       assertEquals(
           'world.com', searchbox.getInputElement().lastInput()?.inline);
     });

 suite('ContextEntrypoint', () => {
   test('CurrentTabChipShown', async () => {
     loadTimeData.overrideValues({
       composeboxShowCurrentTabChip: true,
       composeboxShowChip: true,
     });

     document.body.innerHTML = window.trustedTypes!.emptyHTML;
     searchbox = document.createElement('omnibox-popup-searchbox');
     document.body.appendChild(searchbox);
     searchbox.dropdownIsVisible = true;
     testProxy.initVisibilityPrefs();
     await microtasksFinished();

     const mockTab: TabInfo = {
       tabId: 1,
       title: 'Tab 1',
       url: 'https://tab1.com/',
       showInCurrentTabChip: true,
       showInPreviousTabChip: false,
       isLoading: false,
       lastActive: {internalValue: 1n},
     };

     testProxy.handler.setPromiseResolveFor('getRecentTabs', {tabs: [mockTab]});
     testProxy.page.updateLensSearchEligibility(true);
     testProxy.page.updateContentSharingPolicy(true);

     const result = createAutocompleteResultForTesting({
       input: '',
       matches: [],
     });
     testProxy.page.autocompleteResultChanged(result);
     await microtasksFinished();

     callbackRouter.onShow();
     await testProxy.handler.whenCalled('getRecentTabs');
     await microtasksFinished();

     const popupEntrypoint =
         $$(searchbox, 'omnibox-popup-contextual-entrypoint');
     assertTrue(!!popupEntrypoint);
     const chip = $$(popupEntrypoint, 'composebox-current-tab-chip');
     assertTrue(!!chip);
     assertTrue(isVisible(chip));
   });

   test('LensIconShown', async () => {
     loadTimeData.overrideValues({
       composeboxShowLensIcon: true,
       composeboxShowChip: false,
     });

     document.body.innerHTML = window.trustedTypes!.emptyHTML;
     searchbox = document.createElement('omnibox-popup-searchbox');
     document.body.appendChild(searchbox);
     searchbox.dropdownIsVisible = true;
     testProxy.initVisibilityPrefs();
     await microtasksFinished();

     testProxy.page.updateLensSearchEligibility(true);

     const result = createAutocompleteResultForTesting({
       input: '',
       matches: [],
     });
     testProxy.page.autocompleteResultChanged(result);
     await microtasksFinished();

     callbackRouter.onShow();
     await microtasksFinished();

     const popupEntrypoint =
         $$(searchbox, 'omnibox-popup-contextual-entrypoint');
     assertTrue(!!popupEntrypoint);
     assertTrue(isVisible($$(popupEntrypoint, '#lensSearchIcon')));
   });

   suite('TallSearchbox', () => {
     let localSearchbox: OmniboxPopupSearchboxElement;

     setup(async () => {
       document.body.innerHTML = window.trustedTypes!.emptyHTML;
       loadTimeData.overrideValues({
         omniboxAimPopupEnabled: true,
         hideClassicContextButton: false,
         contextualMenuUsePecApi: false,
         searchboxLayoutMode: 'TallBottomContext',
         composeboxShowChip: true,
       });

       localSearchbox = document.createElement('omnibox-popup-searchbox');
       localSearchbox.dropdownIsVisible = true;
       document.body.appendChild(localSearchbox);
       await microtasksFinished();
       testProxy.initVisibilityPrefs();
       await microtasksFinished();
     });

     test('ContextMenuEntrypointHiddenWhenDisabled', async () => {
       testProxy.page.updateAimPopupEligibility(false);
       await microtasksFinished();
       assertFalse(!!getContextualEntrypointButton(localSearchbox));
     });

     test('ShowContextButtonText', async () => {
       const contextualEntrypoint =
           getContextualEntrypointButton(localSearchbox);
       assertTrue(!!contextualEntrypoint);
       if (contextualEntrypoint) {
         const innerEntrypoint =
             $$(contextualEntrypoint,
                'cr-composebox-contextual-entrypoint-button');
         assertTrue(!!innerEntrypoint);
         assertFalse(!!$$(innerEntrypoint, '#description'));
       }

       document.body.innerHTML = window.trustedTypes!.emptyHTML;
       loadTimeData.overrideValues({
         omniboxAimPopupEnabled: true,
         hideClassicContextButton: false,
         contextualMenuUsePecApi: false,
         composeboxShowContextMenuDescription: true,
         searchboxLayoutMode: 'TallBottomContext',
       });
       localSearchbox = document.createElement('omnibox-popup-searchbox');
       localSearchbox.dropdownIsVisible = true;
       document.body.appendChild(localSearchbox);
       await microtasksFinished();

       testProxy.initVisibilityPrefs();
       testProxy.page.updateAimPopupEligibility(true);
       await microtasksFinished();

       const contextualEntrypoint2 =
           getContextualEntrypointButton(localSearchbox);
       assertTrue(!!contextualEntrypoint2);
       if (contextualEntrypoint2) {
         const newInnerEntrypoint =
             $$(contextualEntrypoint2,
                'cr-composebox-contextual-entrypoint-button');
         assertTrue(!!newInnerEntrypoint);
         const description = $$(newInnerEntrypoint, '#description');
         assertTrue(!!description);
         assertEquals('Add tabs and more', description.textContent.trim());
       }
     });

     test('HideClassicContextButton', async () => {
       const contextualEntrypoint =
           getContextualEntrypointButton(localSearchbox);
       assertTrue(!!contextualEntrypoint);
       assertTrue(isVisible(contextualEntrypoint));

       document.body.innerHTML = window.trustedTypes!.emptyHTML;
       loadTimeData.overrideValues({
         hideClassicContextButton: true,
         contextualMenuUsePecApi: false,
       });
       localSearchbox = document.createElement('omnibox-popup-searchbox');
       localSearchbox.dropdownIsVisible = true;
       document.body.appendChild(localSearchbox);
       await microtasksFinished();

       testProxy.initVisibilityPrefs();
       testProxy.page.updateAimPopupEligibility(true);
       await microtasksFinished();

       assertFalse(!!getContextualEntrypointButton(localSearchbox));
     });

     test(
         'DropdownVisibleWhenHasContextualChipsEvenWithoutMatches',
         async () => {
           localSearchbox.dropdownIsVisible = false;
           testProxy.page.updateAimPopupEligibility(true);
           testProxy.page.updateLensSearchEligibility(true);
           await microtasksFinished();

           assertTrue(localSearchbox.dropdownIsVisible);
           const contextualEntrypoint =
               getContextualEntrypointButton(localSearchbox);
           assertTrue(!!contextualEntrypoint);
           assertTrue(isVisible(contextualEntrypoint));
         });

     test('DropdownHiddenWhenNotLensEligibleAndNoMatches', async () => {
       localSearchbox.dropdownIsVisible = false;
       testProxy.page.updateAimPopupEligibility(true);
       testProxy.page.updateLensSearchEligibility(false);
       await microtasksFinished();

       assertFalse(localSearchbox.dropdownIsVisible);
       const contextualEntrypoint =
           getContextualEntrypointButton(localSearchbox);
       assertFalse(!!contextualEntrypoint);
     });

     test('DropdownHiddenWhenAskGShowChipDisabledAndNoMatches', async () => {
       document.body.innerHTML = window.trustedTypes!.emptyHTML;
       loadTimeData.overrideValues({composeboxShowChip: false});
       localSearchbox = document.createElement('omnibox-popup-searchbox');
       localSearchbox.dropdownIsVisible = false;
       document.body.appendChild(localSearchbox);
       await microtasksFinished();

       testProxy.page.updateAimPopupEligibility(true);
       testProxy.page.updateLensSearchEligibility(true);
       await microtasksFinished();

       assertFalse(localSearchbox.dropdownIsVisible);
       const contextualEntrypoint =
           getContextualEntrypointButton(localSearchbox);
       assertFalse(!!contextualEntrypoint);
     });

     test('DropdownSuppressedWhenMultilineInputGrows', async () => {
       localSearchbox.multiLineEnabled = true;
       const inputElement = localSearchbox.getInputElement();
       inputElement.multiLineEnabled = true;
       await microtasksFinished();
       localSearchbox.result = createAutocompleteResultForTesting({
         input: 'hello world',
         matches: [],
       });
       Object.defineProperty(
           inputElement.shadowRoot.querySelector<HTMLElement>('#input')!,
           'scrollHeight', {value: 60, configurable: true});
       testProxy.page.updateAimPopupEligibility(true);
       testProxy.page.updateLensSearchEligibility(true);
       await microtasksFinished();

       assertFalse(localSearchbox.dropdownIsVisible);
       const contextualEntrypoint =
           getContextualEntrypointButton(localSearchbox);
       assertFalse(!!contextualEntrypoint);
     });
   });

   suite('AimEligibility', () => {
     let localSearchbox: OmniboxPopupSearchboxElement;

     setup(async () => {
       document.body.innerHTML = window.trustedTypes!.emptyHTML;
       loadTimeData.overrideValues({
         hideClassicContextButton: false,
         contextualMenuUsePecApi: false,
         searchboxLayoutMode: 'TallBottomContext',
       });
       localSearchbox = document.createElement('omnibox-popup-searchbox');
       localSearchbox.dropdownIsVisible = true;
       document.body.appendChild(localSearchbox);
       await microtasksFinished();

       testProxy.initVisibilityPrefs();
       await microtasksFinished();
     });

     test('AimEligibility', async () => {
       testProxy.page.updateAimPopupEligibility(false);
       await microtasksFinished();
       let contextualEntrypoint = getContextualEntrypointButton(localSearchbox);
       assertFalse(!!contextualEntrypoint);

       testProxy.page.updateAimPopupEligibility(true);
       await microtasksFinished();
       contextualEntrypoint = getContextualEntrypointButton(localSearchbox);
       assertTrue(!!contextualEntrypoint);
       assertTrue(isVisible(contextualEntrypoint));

       testProxy.page.updateAimPopupEligibility(false);
       await microtasksFinished();
       contextualEntrypoint = getContextualEntrypointButton(localSearchbox);
       assertFalse(!!contextualEntrypoint);
     });

     test('DisallowedInputsHidesEntrypoint', async () => {
       document.body.innerHTML = window.trustedTypes!.emptyHTML;
       loadTimeData.overrideValues({
         hideClassicContextButton: false,
         contextualMenuUsePecApi: true,
         searchboxLayoutMode: 'TallBottomContext',
       });
       localSearchbox = document.createElement('omnibox-popup-searchbox');
       document.body.appendChild(localSearchbox);
       localSearchbox.dropdownIsVisible = true;

       testProxy.initVisibilityPrefs();
       testProxy.page.updateAimPopupEligibility(true);
       await microtasksFinished();

       testProxy.page.onInputStateChanged({
         ...createDefaultInputState(),
         allowedModels: [],
         allowedTools: [],
         allowedInputTypes: [],
       });
       await microtasksFinished();

       const popupEntrypoint =
           $$(localSearchbox, 'omnibox-popup-contextual-entrypoint');
       assertTrue(!!popupEntrypoint);
       const contextualEntrypoint =
           $$(popupEntrypoint, 'omnibox-popup-contextual-entrypoint-button');
       assertFalse(!!contextualEntrypoint);
     });
   });
 });

 test('HandlesClearPopup', async () => {
   // Populate autocomplete result to show dropdown.
   searchbox.onAutocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: searchbox.activeQueryId,
     sequenceId: 123,
     input: 'test',
     matches: [createSearchMatchForTesting()],
   }));
   await microtasksFinished();
   assertTrue(searchbox.dropdownIsVisible);

   // Trigger clearPopup callback from Mojo.
   callbackRouter.clearPopup();
   await microtasksFinished();

   // Ensure result is null, dropdown is closed, and input text is cleared.
   assertFalse(searchbox.dropdownIsVisible);
   assertFalse(!!searchbox.result);
   assertEquals('', searchbox.$.input.lastInput()?.text ?? '');
 });

 test('ClearsAutocompleteMatchesOnSetInputState', async () => {
   // Populate autocomplete result to show dropdown.
   searchbox.onAutocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: searchbox.activeQueryId,
     sequenceId: 123,
     input: 'test',
     matches: [createSearchMatchForTesting()],
   }));
   await microtasksFinished();
   assertTrue(searchbox.dropdownIsVisible);

   // Update input state via Mojo (simulating tab switch / state reset).
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: 'new tab input',
     isFocused: true,
     isTabSwitch: true,
   }));
   await microtasksFinished();

   // Ensure matches and dropdown are cleared for the new input state.
   assertFalse(searchbox.dropdownIsVisible);
   assertFalse(!!searchbox.result);

   // Populate autocomplete result again on Tab 2.
   searchbox.onAutocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: searchbox.activeQueryId,
     sequenceId: 124,
     input: 'test2',
     matches: [createSearchMatchForTesting()],
   }));
   await microtasksFinished();
   assertTrue(searchbox.dropdownIsVisible);

   // Update input state to enter keyword mode.
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     tabId: 2,
     text: 'test2',
     isFocused: true,
     keywordModel: {
       type: KeywordType.kInKeyword,
       keyword: 'google.com',
       displayText: 'Search Google',
       iconPath: '',
       placeholder: '',
     },
   }));
   await microtasksFinished();

   // Ensure matches are cleared on keyword change.
   assertFalse(searchbox.dropdownIsVisible);
   assertFalse(!!searchbox.result);
 });

 suite('InputIconState', () => {
   test('InputMatchUpdatesWithSelectedMatch', async () => {
     const navMatch = createSearchMatchForTesting({
       suggestTemplate: createSuggestTemplateInfo({primaryText: 'example.com'}),
       destinationUrl: 'https://example.com/',
       isSearchType: false,
       type: 'history-url',
     });
     const searchMatch = createSearchMatchForTesting({
       suggestTemplate:
           createSuggestTemplateInfo({primaryText: 'example query'}),
       destinationUrl: 'https://www.google.com/search?q=example',
       isSearchType: true,
       type: 'search-what-you-typed',
     });

     searchbox.activeQueryId = 0;
     const result = createAutocompleteResultForTesting({
       input: 'example',
       matches: [navMatch, searchMatch],
       queryId: 0,
     });
     testProxy.page.autocompleteResultChanged(result);
     await microtasksFinished();

     // With selectedMatchIndex === -1 while typing, input's selectedMatch
     // should be null.
     assertEquals(-1, searchbox.selectedMatchIndex);
     assertEquals(null, searchbox.$.input.selectedMatch);

     // When navigating to match 1, input's selectedMatch should be match 1.
     searchbox.selectedMatchIndex = 1;
     await microtasksFinished();
     assertEquals(
         searchbox.result!.matches[1], searchbox.$.input.selectedMatch);
     assertEquals(
         'example query',
         searchbox.$.input.selectedMatch?.suggestTemplate.primaryText.text);

     // When navigating to match 0, input's selectedMatch should be match 0.
     searchbox.selectedMatchIndex = 0;
     await microtasksFinished();
     assertEquals(
         searchbox.result!.matches[0], searchbox.$.input.selectedMatch);
     assertEquals(
         'example.com',
         searchbox.$.input.selectedMatch?.suggestTemplate.primaryText.text);

     // When navigating back to unselected, input's selectedMatch should return
     // to null.
     searchbox.selectedMatchIndex = -1;
     await microtasksFinished();
     assertEquals(null, searchbox.$.input.selectedMatch);
   });

   test('InputMatchClearsWhenCutEmptiesInput', async () => {
     const navMatch = createSearchMatchForTesting({
       suggestTemplate: createSuggestTemplateInfo({primaryText: 'example.com'}),
       destinationUrl: 'https://example.com/',
       isSearchType: false,
       type: 'history-url',
     });
     const searchMatch = createSearchMatchForTesting({
       suggestTemplate:
           createSuggestTemplateInfo({primaryText: 'example query'}),
       destinationUrl: 'https://www.google.com/search?q=example',
       isSearchType: true,
       type: 'search-what-you-typed',
     });

     searchbox.activeQueryId = 0;
     const result = createAutocompleteResultForTesting({
       input: 'example',
       matches: [navMatch, searchMatch],
       queryId: 0,
     });
     testProxy.page.autocompleteResultChanged(result);
     await microtasksFinished();

     // With selectedMatchIndex === -1 while typing, input's selectedMatch
     // should be null.
     assertEquals(-1, searchbox.selectedMatchIndex);
     assertEquals(null, searchbox.$.input.selectedMatch);

     // When navigating to match 0, input's selectedMatch should be match 0.
     searchbox.selectedMatchIndex = 0;
     await microtasksFinished();
     assertEquals(
         searchbox.result!.matches[0], searchbox.$.input.selectedMatch);
     assertEquals(
         'example.com',
         searchbox.$.input.selectedMatch?.suggestTemplate.primaryText.text);

     searchbox.clearAutocompleteMatches();  // Closes the popup, as Esc does.
     await microtasksFinished();

     const input = searchbox.$.input.inputElement;
     input.setSelectionRange(0, input.value.length);
     searchbox.$.input.dispatchEvent(new ClipboardEvent(
         'cut', {bubbles: true, cancelable: true, composed: true}));
     await microtasksFinished();
     assertEquals(null, searchbox.$.input.selectedMatch, 'after cut');

     // Typing again before results arrive shouldn't bring the old match back.
     input.value = 'g';
     input.setSelectionRange(1, 1);
     input.dispatchEvent(
         new InputEvent('input', {bubbles: true, composed: true}));
     await microtasksFinished();
     assertEquals(null, searchbox.$.input.selectedMatch, 'after retyping');
   });

   test(
       'SelectedMatchPreservedForInputOnBlurWhenUserInputInProgress',
       async () => {
         callbackRouter.setInputState(createDefaultOmniboxInputState({
           isFocused: true,
           userInputInProgress: true,
           text: 'example',
           permanentDisplayText: 'https://example.com/',
           fullUrl: 'https://example.com/',
         }));
         await microtasksFinished();

         const navMatch = createSearchMatchForTesting({
           suggestTemplate:
               createSuggestTemplateInfo({primaryText: 'example.com'}),
           destinationUrl: 'https://example.com/',
           allowedToBeDefaultMatch: true,
           isSearchType: false,
         });
         searchbox.activeQueryId = 0;
         const result = createAutocompleteResultForTesting({
           input: 'example',
           matches: [navMatch],
           queryId: 0,
         });
         testProxy.page.autocompleteResultChanged(result);
         await microtasksFinished();

         searchbox.selectedMatchIndex = 0;
         await microtasksFinished();
         assertEquals(
             'example.com',
             searchbox.$.input.selectedMatch?.suggestTemplate.primaryText.text);

         // Defocus / blur the omnibox (handleFocusLost_ clears autocomplete
         // matches).
         callbackRouter.setFocus(
             false, /*queryZps=*/ false, /*selectAll=*/ false,
             /*showFullUrl=*/ false);
         await microtasksFinished();

         // Verify autocomplete result was cleared on the searchbox component.
         assertEquals(null, searchbox.result);
         assertEquals(null, searchbox.selectedMatch);

         // But input's selectedMatch should be preserved for the icon because
         // user input is still in progress.
         assertEquals(
             'example.com',
             searchbox.$.input.selectedMatch?.suggestTemplate.primaryText.text);

         // When reverting (user input is no longer in progress), selectedMatch
         // for input is cleared.
         callbackRouter.setInputState(createDefaultOmniboxInputState({
           isFocused: false,
           userInputInProgress: false,
           text: 'https://example.com/',
         }));
         await microtasksFinished();
         callbackRouter.setInputState(createDefaultOmniboxInputState(
             {isFocused: true, userInputInProgress: true, text: 'e'}));
         await microtasksFinished();
         assertEquals(null, searchbox.$.input.selectedMatch);
       });

   test(
       'SelectedMatchRestoredForInputOnTabSwitchWithCurrentMatch', async () => {
         const navMatch = createSearchMatchForTesting({
           suggestTemplate:
               createSuggestTemplateInfo({primaryText: 'espn.com'}),
           destinationUrl: 'https://espn.com/',
           allowedToBeDefaultMatch: true,
           isSearchType: false,
         });

         // Simulate restoring state from a tab switch where user input was in
         // progress and currentMatch is provided by C++ OmniboxInputState.
         callbackRouter.setInputState(createDefaultOmniboxInputState({
           isFocused: true,
           userInputInProgress: true,
           isTabSwitch: true,
           text: 'espn.com',
           permanentDisplayText: 'https://google.com/',
           fullUrl: 'https://google.com/',
           currentMatch: navMatch,
         }));
         await microtasksFinished();

         // Autocomplete result should be cleared on tab switch.
         assertEquals(null, searchbox.result);
         assertEquals(null, searchbox.selectedMatch);

         // But input's selectedMatch should be restored from currentMatch for
         // the icon since user input is in progress.
         assertEquals(
             'espn.com',
             searchbox.$.input.selectedMatch?.suggestTemplate.primaryText.text);

         // When reverting, selectedMatch for input is cleared.
         callbackRouter.setInputState(createDefaultOmniboxInputState({
           isFocused: false,
           userInputInProgress: false,
           text: 'https://google.com/',
         }));
         await microtasksFinished();
         callbackRouter.setInputState(createDefaultOmniboxInputState(
             {isFocused: true, userInputInProgress: true, text: 'e'}));
         await microtasksFinished();
         assertEquals(null, searchbox.$.input.selectedMatch);
       });

   test('UneditedPageUrlProvidesPageUrl', async () => {
     // When input is unedited on a regular webpage URL (e.g. badssl or initial
     // page focus):
     callbackRouter.setInputState(createDefaultOmniboxInputState({
       userInputInProgress: false,
       permanentDisplayText: 'https://expired.badssl.com/',
       fullUrl: 'https://expired.badssl.com/',
     }));
     await microtasksFinished();

     // pageUrl should be passed to input element while selectedMatch is null.
     assertEquals('https://expired.badssl.com/', searchbox.$.input.pageUrl);
     assertEquals(null, searchbox.$.input.selectedMatch);

     // On NTP, pageUrl should be empty (to show Super G default icon).
     callbackRouter.setInputState(createDefaultOmniboxInputState({
       userInputInProgress: false,
       permanentDisplayText: 'chrome://newtab',
       fullUrl: 'chrome://newtab/',
     }));
     await microtasksFinished();
     assertEquals('', searchbox.$.input.pageUrl);
     assertEquals(null, searchbox.$.input.selectedMatch);

     // When the user starts typing (userInputInProgress becomes true), pageUrl
     // becomes empty.
     callbackRouter.setInputState(createDefaultOmniboxInputState({
       userInputInProgress: true,
       text: 'search query',
     }));
     await microtasksFinished();
     assertEquals('', searchbox.$.input.pageUrl);
     assertEquals(null, searchbox.$.input.selectedMatch);
   });

   test('HandlesSetDefaultSearchProvider', async () => {
     const customIcon =
         'chrome://favicon2/?iconUrl=https%3A%2F%2Fexample.com%2Ffavicon.ico';
     callbackRouter.setDefaultSearchProvider(customIcon);
     await microtasksFinished();
     assertEquals(customIcon, searchbox.$.input.searchboxIcon);
   });
 });

 test('TabKeyAcceptsInlineAutocomplete', async () => {
   searchbox.virtualFocusEnabled = false;
   searchbox.dropdownIsVisible = true;
   searchbox.focusInput();
   searchbox.getInputElement().setInput({
     text: 'you',
     inline: 'tube.com',
   });
   await microtasksFinished();

   const tabEvent = new KeyboardEvent('keydown', {
     key: 'Tab',
     cancelable: true,
     bubbles: true,
   });
   await searchbox.handleKeyNavigation(tabEvent);
   await microtasksFinished();

   assertTrue(tabEvent.defaultPrevented);
   assertEquals('youtube.com', searchbox.getInputElement().inputElement.value);
   assertEquals(11, searchbox.getInputElement().inputElement.selectionStart);
   assertEquals(11, searchbox.getInputElement().inputElement.selectionEnd);
   assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
   const [
     _queryId,
     _tabId,
     queryText,
     preventInline,
     cursorPos,
     _inventory,
     isOnFocus,
   ] = testProxy.handler.getArgs('queryAutocomplete')[0];
   assertEquals('youtube.com', queryText);
   assertFalse(preventInline);
   assertEquals(11, cursorPos);
   assertFalse(isOnFocus);
 });

 test('ShiftTabClearsInlineAutocompleteWithoutPreventDefault', async () => {
   searchbox.virtualFocusEnabled = false;
   searchbox.dropdownIsVisible = true;
   searchbox.focusInput();
   searchbox.getInputElement().setInput({
     text: 'you',
     inline: 'tube.com',
   });
   await microtasksFinished();

   const shiftTabEvent = new KeyboardEvent('keydown', {
     key: 'Tab',
     shiftKey: true,
     cancelable: true,
     bubbles: true,
   });
   await searchbox.handleKeyNavigation(shiftTabEvent);
   await microtasksFinished();

   assertFalse(shiftTabEvent.defaultPrevented);
   const lastInput = searchbox.getInputElement().lastInput();
   assertTrue(!!lastInput);
   assertEquals('', lastInput.inline);
   assertEquals('you', lastInput.text);
 });

 test('TabKeyPrioritizesKeywordEntryOverInlineAutocomplete', async () => {
   searchbox.virtualFocusEnabled = false;
   const keyword = 'youtube.com';
   const match = createSearchMatchForTesting({
     allowedToBeDefaultMatch: true,
     suggestTemplate: createSuggestTemplateInfo({primaryText: 'youtube'}),
     keywordModel: createMatchKeywordModelForTesting({
       type: KeywordType.kChip,
       keyword,
       chipHint: 'Search YouTube',
     }),
   });
   searchbox.activeQueryId = 0;
   searchbox.onAutocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: 0,
     input: 'you',
     matches: [match],
   }));
   await microtasksFinished();

   searchbox.focusInput();
   searchbox.getInputElement().setInput({
     text: 'you',
     inline: 'tube.com',
   });
   await microtasksFinished();

   const tabEvent = new KeyboardEvent('keydown', {
     key: 'Tab',
     cancelable: true,
     bubbles: true,
   });
   await searchbox.handleKeyNavigation(tabEvent);
   await microtasksFinished();

   assertTrue(tabEvent.defaultPrevented);
   assertTrue(searchbox.inputKeywordModel !== null);
   assertEquals(KeywordType.kInKeyword, searchbox.inputKeywordModel.type);
   assertEquals(keyword, searchbox.inputKeywordModel.keyword);
   assertEquals('', searchbox.getInputElement().inputElement.value);
 });

 test('TabKeyFallsBackToInlineAutocompleteWhenNoKeywordChip', async () => {
   searchbox.virtualFocusEnabled = false;
   const match = createSearchMatchForTesting({
     allowedToBeDefaultMatch: true,
     suggestTemplate: createSuggestTemplateInfo({primaryText: 'youtube.com'}),
   });
   searchbox.activeQueryId = 0;
   searchbox.onAutocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: 0,
     input: 'you',
     matches: [match],
   }));
   await microtasksFinished();

   searchbox.focusInput();
   searchbox.getInputElement().setInput({
     text: 'you',
     inline: 'tube.com',
   });
   await microtasksFinished();

   const tabEvent = new KeyboardEvent('keydown', {
     key: 'Tab',
     cancelable: true,
     bubbles: true,
   });
   await searchbox.handleKeyNavigation(tabEvent);
   await microtasksFinished();

   assertTrue(tabEvent.defaultPrevented);
   assertEquals(null, searchbox.inputKeywordModel);
   assertEquals('youtube.com', searchbox.getInputElement().inputElement.value);
 });

 test('FocusLostHidesAimButton', async () => {
   // Explicitly set focus and enable AIM button visibility.
   callbackRouter.setFocus(
       true, /*queryZps=*/ false, /*selectAll=*/ false, /*showFullUrl=*/ false);
   testProxy.page.setAimButtonVisible(true);
   await microtasksFinished();

   const composeButton = searchbox.$.composeButton;
   assertTrue(!!composeButton);
   assertTrue(isVisible(composeButton));

   // When focus is lost, AIM button should be hidden.
   callbackRouter.setFocus(
       false, /*queryZps=*/ false, /*selectAll=*/ false,
       /*showFullUrl=*/ false);
   await microtasksFinished();

   assertFalse(isVisible(composeButton));

   // Refocus, then type text into the Omnibox.
   callbackRouter.setFocus(
       true, /*queryZps=*/ true, /*selectAll=*/ false, /*showFullUrl=*/ false);
   searchbox.getInputElement().setInputText('temporary text');
   testProxy.page.setAimButtonVisible(true);
   await microtasksFinished();

   assertTrue(isVisible(composeButton));

   // If focus is lost with temporary text in the Omnibox, then AIM button
   // should be hidden.
   callbackRouter.setFocus(
       false, /*queryZps=*/ false, /*selectAll=*/ false,
       /*showFullUrl=*/ false);
   await microtasksFinished();

   assertFalse(isVisible(composeButton));
   assertEquals(
       'temporary text', searchbox.getInputElement().inputElement.value);
 });

 test('TabKeyWithVirtualFocusNavigatesToKeywordChip', async () => {
   loadTimeData.overrideValues({realboxVirtualFocusNavigation: true});
   searchbox.virtualFocusEnabled = true;

   const match = createSearchMatchForTesting({
     allowedToBeDefaultMatch: true,
     fillIntoEdit: 'youtube.com',
     keywordModel: createMatchKeywordModelForTesting({
       type: KeywordType.kChip,
       keyword: 'youtube.com',
       chipHint: 'Search YouTube',
     }),
   });
   searchbox.activeQueryId = 0;
   searchbox.onAutocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: 0,
     input: 'youtube.com',
     matches: [match],
   }));
   await microtasksFinished();

   searchbox.focusInput();
   const tabEvent = new KeyboardEvent('keydown', {
     key: 'Tab',
     cancelable: true,
     bubbles: true,
   });
   await searchbox.handleKeyNavigation(tabEvent);
   await microtasksFinished();

   assertEquals(0, searchbox.selection.line);
   assertEquals(SelectionLineState.kKeywordMode, searchbox.selection.state);
   assertTrue(searchbox.keywordModeManager.isInKeywordMode);
   assertEquals('youtube.com', searchbox.inputKeywordModel?.keyword);
   assertEquals('', searchbox.getInputElement().inputElement.value);
 });

 test('AimButtonUserInputState', async () => {
   const composeButton = searchbox.$.composeButton;
   assertTrue(!!composeButton);

   // A prefilled URL without user input in progress should not set
   // has-user-input.
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: 'https://example.com',
     userInputInProgress: false,
   }));
   await microtasksFinished();
   assertFalse(composeButton.hasAttribute('has-user-input'));

   // When input is actively entered by the user, has-user-input should be set.
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: 'https://example.com/search',
     userInputInProgress: true,
   }));
   await microtasksFinished();
   assertTrue(composeButton.hasAttribute('has-user-input'));
 });

 test('EnterKeyWithVirtualFocusOnRemoveSuggestionDeletesMatch', async () => {
   loadTimeData.overrideValues({realboxVirtualFocusNavigation: true});
   searchbox.virtualFocusEnabled = true;

   const match = createSearchMatchForTesting({
     destinationUrl: 'https://example.com/delete',
     supportsDeletion: true,
   });
   searchbox.activeQueryId = 0;
   searchbox.onAutocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: 0,
     input: 'delete query',
     matches: [match],
   }));
   await microtasksFinished();

   searchbox.setSelection({
     line: 0,
     state: SelectionLineState.kFocusedButtonRemoveSuggestion,
     actionIndex: 0,
   });
   await microtasksFinished();

   const enterEvent =
       new KeyboardEvent('keydown', {key: 'Enter', cancelable: true});
   await searchbox.handleKeyNavigation(enterEvent);
   await microtasksFinished();

   assertTrue(enterEvent.defaultPrevented);
   assertEquals(1, testProxy.handler.getCallCount('deleteAutocompleteMatch'));
   const [line, url] =
       await testProxy.handler.whenCalled('deleteAutocompleteMatch');
   assertEquals(0, line);
   assertEquals('https://example.com/delete', url);
   assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));
 });

 test('SelectionStaysOnDeletedLineAfterMatchDeletion', async () => {
   const matches = [0, 1, 2, 3].map(i => {
     return createSearchMatchForTesting({
       allowedToBeDefaultMatch: i === 0,
       fillIntoEdit: `query ${i}`,
       destinationUrl: `https://example.com/${i}`,
       supportsDeletion: true,
     });
   });
   searchbox.activeQueryId = 0;
   searchbox.onAutocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: 0,
     input: 'query',
     matches,
   }));
   await microtasksFinished();

   searchbox.setSelection({
     line: 2,
     state: SelectionLineState.kFocusedButtonRemoveSuggestion,
     actionIndex: 0,
   });
   await microtasksFinished();
   await searchbox.handleKeyNavigation(
       new KeyboardEvent('keydown', {key: 'Enter', cancelable: true}));
   const [line, url] =
       await testProxy.handler.whenCalled('deleteAutocompleteMatch');
   assertEquals(2, line);
   assertEquals('https://example.com/2', url);

   // Like the native omnibox, the selection stays on the deleted line rather
   // than moving to the default match.
   searchbox.onAutocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: searchbox.activeQueryId,
     input: 'query',
     matches: matches.filter((_, i) => i !== 2),
   }));
   await microtasksFinished();

   assertDeepEquals(
       {line: 2, state: SelectionLineState.kNormal, actionIndex: 0},
       searchbox.selection);
   assertEquals(2, searchbox.selectedMatchIndex);
   assertEquals('query 3', searchbox.$.input.inputElement.value);
 });

 test(
     'EnterKeyWithVirtualFocusOnContextualEntrypointOpensContextMenu',
     async () => {
       loadTimeData.overrideValues({realboxVirtualFocusNavigation: true});
       searchbox.virtualFocusEnabled = true;
       searchbox.dropdownIsVisible = true;
       testProxy.initVisibilityPrefs();
       testProxy.page.updateAimPopupEligibility(true);
       searchbox.setSelection({
         line: -1,
         state: SelectionLineState.kFocusedButtonContextEntrypoint,
         actionIndex: 0,
       });
       await microtasksFinished();

       await searchbox.handleKeyNavigation(
           new KeyboardEvent('keydown', {key: 'Enter', cancelable: true}));
       await microtasksFinished();

       assertEquals(1, handler.getCallCount('showContextMenu'));
       assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));
     });

 test('EnterKeyWithVirtualFocusOnLensSearchIconOpensLensSearch', async () => {
   document.body.innerHTML = window.trustedTypes!.emptyHTML;
   loadTimeData.overrideValues({
     realboxVirtualFocusNavigation: true,
     hideClassicContextButton: false,
     contextualMenuUsePecApi: false,
     searchboxLayoutMode: 'TallBottomContext',
     composeboxShowChip: false,
     composeboxShowLensIcon: true,
   });
   const localSearchbox = document.createElement('omnibox-popup-searchbox');
   localSearchbox.dropdownIsVisible = true;
   localSearchbox.virtualFocusEnabled = true;
   document.body.appendChild(localSearchbox);
   await microtasksFinished();

   testProxy.initVisibilityPrefs();
   testProxy.page.updateAimPopupEligibility(true);
   testProxy.page.updateLensSearchEligibility(true);
   await microtasksFinished();

   assertTrue(localSearchbox.showLensSearchIcon);
   localSearchbox.setSelection({
     line: -1,
     state: SelectionLineState.kFocusedButtonLensSearch,
     actionIndex: 0,
   });
   await microtasksFinished();

   await localSearchbox.handleKeyNavigation(
       new KeyboardEvent('keydown', {key: 'Enter', cancelable: true}));
   await microtasksFinished();

   assertEquals(1, testProxy.handler.getCallCount('openLensSearch'));
   assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));
 });

 test('EnterKeyWithVirtualFocusOnLensSearchChipOpensLensSearch', async () => {
   document.body.innerHTML = window.trustedTypes!.emptyHTML;
   loadTimeData.overrideValues({
     realboxVirtualFocusNavigation: true,
     hideClassicContextButton: false,
     contextualMenuUsePecApi: false,
     searchboxLayoutMode: 'TallBottomContext',
     composeboxShowChip: true,
     composeboxShowLensIcon: false,
   });
   const localSearchbox = document.createElement('omnibox-popup-searchbox');
   localSearchbox.dropdownIsVisible = true;
   localSearchbox.virtualFocusEnabled = true;
   document.body.appendChild(localSearchbox);
   await microtasksFinished();

   testProxy.initVisibilityPrefs();
   testProxy.page.updateAimPopupEligibility(true);
   testProxy.page.updateLensSearchEligibility(true);
   await microtasksFinished();

   assertTrue(localSearchbox.showContextualChip);
   localSearchbox.setSelection({
     line: -1,
     state: SelectionLineState.kFocusedButtonContextualChip,
     actionIndex: 0,
   });
   await microtasksFinished();

   await localSearchbox.handleKeyNavigation(
       new KeyboardEvent('keydown', {key: 'Enter', cancelable: true}));
   await microtasksFinished();

   assertEquals(1, testProxy.handler.getCallCount('openLensSearch'));
   assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));
 });

 test('EnterKeyWithVirtualFocusOnCurrentTabChipAddsTabContext', async () => {
   document.body.innerHTML = window.trustedTypes!.emptyHTML;
   loadTimeData.overrideValues({
     realboxVirtualFocusNavigation: true,
     hideClassicContextButton: false,
     contextualMenuUsePecApi: false,
     searchboxLayoutMode: 'TallBottomContext',
     composeboxShowChip: true,
     composeboxShowCurrentTabChip: true,
     composeboxShowLensIcon: false,
   });
   testProxy.handler.setPromiseResolveFor<'getRecentTabs'>('getRecentTabs', {
     tabs: [{
       tabId: 42,
       title: 'Test Tab',
       url: 'https://example.com',
       showInCurrentTabChip: true,
       showInPreviousTabChip: false,
       lastActive: {internalValue: 100n},
     }],
   });

   const localSearchbox = document.createElement('omnibox-popup-searchbox');
   localSearchbox.dropdownIsVisible = true;
   localSearchbox.virtualFocusEnabled = true;
   document.body.appendChild(localSearchbox);
   await microtasksFinished();

   testProxy.initVisibilityPrefs();
   testProxy.page.updateAimPopupEligibility(true);
   testProxy.page.updateLensSearchEligibility(true);
   callbackRouter.onShow();
   await testProxy.handler.whenCalled('getRecentTabs');
   await microtasksFinished();

   assertTrue(localSearchbox.showContextualChip);
   assertTrue(!!getCurrentTabChip(localSearchbox));
   localSearchbox.setSelection({
     line: -1,
     state: SelectionLineState.kFocusedButtonContextualChip,
     actionIndex: 0,
   });
   await microtasksFinished();

   await localSearchbox.handleKeyNavigation(
       new KeyboardEvent('keydown', {key: 'Enter', cancelable: true}));
   await microtasksFinished();

   assertEquals(1, testProxy.handler.getCallCount('addTabContext'));
   const [tabId, delayUpload] =
       await testProxy.handler.whenCalled('addTabContext');
   assertEquals(42, tabId);
   assertFalse(delayUpload);
   assertEquals(0, testProxy.handler.getCallCount('openAutocompleteMatch'));
 });

 test('stepCyclesSelection always returns false', () => {
   const selection = {
     line: 0,
     state: SelectionLineState.kNormal,
     actionIndex: 0,
   };
   assertFalse(searchbox.stepCyclesSelection(
       null, selection, SelectionDirection.kForward, SelectionStep.kWholeLine));
   assertFalse(searchbox.stepCyclesSelection(
       null, selection, SelectionDirection.kBackward,
       SelectionStep.kWholeLine));

   const contextSelection = {
     line: -1,
     state: SelectionLineState.kFocusedButtonContextEntrypoint,
     actionIndex: 0,
   };
   assertFalse(searchbox.stepCyclesSelection(
       null, contextSelection, SelectionDirection.kForward,
       SelectionStep.kWholeLine));
   assertFalse(searchbox.stepCyclesSelection(
       null, contextSelection, SelectionDirection.kBackward,
       SelectionStep.kWholeLine));
 });

 test(
     'ShiftTabWithVirtualFocusCyclesBackwardsThroughMatchesAndContextualEntrypoint',
     async () => {
       document.body.innerHTML = window.trustedTypes!.emptyHTML;
       loadTimeData.overrideValues({
         realboxVirtualFocusNavigation: true,
         hideClassicContextButton: false,
         contextualMenuUsePecApi: false,
         searchboxLayoutMode: 'TallBottomContext',
       });
       const localSearchbox = document.createElement('omnibox-popup-searchbox');
       localSearchbox.dropdownIsVisible = true;
       localSearchbox.virtualFocusEnabled = true;
       document.body.appendChild(localSearchbox);
       await microtasksFinished();

       testProxy.initVisibilityPrefs();
       testProxy.page.updateAimPopupEligibility(true);
       await microtasksFinished();

       const match1 = createSearchMatchForTesting({
         allowedToBeDefaultMatch: true,
         suggestTemplate: createSuggestTemplateInfo({primaryText: 'match 1'}),
       });
       const match2 = createSearchMatchForTesting({
         allowedToBeDefaultMatch: false,
         suggestTemplate: createSuggestTemplateInfo({primaryText: 'match 2'}),
       });
       localSearchbox.activeQueryId = 0;
       localSearchbox.onAutocompleteResultChanged(
           createAutocompleteResultForTesting({
             queryId: 0,
             input: 'test',
             matches: [match1, match2],
           }));
       await microtasksFinished();

       assertTrue(localSearchbox.showContextEntrypoint);
       const entrypointButton = getContextualEntrypointButton(localSearchbox);
       assertTrue(!!entrypointButton);
       assertFalse(entrypointButton.hasVirtualFocus);

       // Start at match 0 (the default match).
       localSearchbox.setSelection({
         line: 0,
         state: SelectionLineState.kNormal,
         actionIndex: 0,
       });
       await microtasksFinished();

       // 1st Shift-Tab: cycles backward from match 0 to contextual entrypoint.
       const shiftTab1 = new KeyboardEvent('keydown', {
         key: 'Tab',
         shiftKey: true,
         cancelable: true,
         bubbles: true,
       });
       await localSearchbox.handleKeyNavigation(shiftTab1);
       await microtasksFinished();

       assertTrue(shiftTab1.defaultPrevented);
       assertEquals(-1, localSearchbox.selection.line);
       assertEquals(
           SelectionLineState.kFocusedButtonContextEntrypoint,
           localSearchbox.selection.state);
       assertTrue(localSearchbox.isContextEntrypointVirtualFocused());
       assertTrue(entrypointButton.hasVirtualFocus);

       // 2nd Shift-Tab: moves backward from contextual entrypoint to match 1.
       const shiftTab2 = new KeyboardEvent('keydown', {
         key: 'Tab',
         shiftKey: true,
         cancelable: true,
         bubbles: true,
       });
       await localSearchbox.handleKeyNavigation(shiftTab2);
       await microtasksFinished();

       assertTrue(shiftTab2.defaultPrevented);
       assertEquals(1, localSearchbox.selection.line);
       assertEquals(SelectionLineState.kNormal, localSearchbox.selection.state);
       assertFalse(localSearchbox.isContextEntrypointVirtualFocused());
       assertFalse(entrypointButton.hasVirtualFocus);

       // 3rd Shift-Tab: moves backward from match 1 to match 0.
       const shiftTab3 = new KeyboardEvent('keydown', {
         key: 'Tab',
         shiftKey: true,
         cancelable: true,
         bubbles: true,
       });
       await localSearchbox.handleKeyNavigation(shiftTab3);
       await microtasksFinished();

       assertTrue(shiftTab3.defaultPrevented);
       assertEquals(0, localSearchbox.selection.line);
       assertEquals(SelectionLineState.kNormal, localSearchbox.selection.state);

       // Forward Tab: moves forward from match 0 to match 1.
       const tab1 = new KeyboardEvent('keydown', {
         key: 'Tab',
         shiftKey: false,
         cancelable: true,
         bubbles: true,
       });
       await localSearchbox.handleKeyNavigation(tab1);
       await microtasksFinished();

       assertTrue(tab1.defaultPrevented);
       assertEquals(1, localSearchbox.selection.line);

       // 2nd Forward Tab: moves forward from match 1 to contextual entrypoint.
       const tab2 = new KeyboardEvent('keydown', {
         key: 'Tab',
         shiftKey: false,
         cancelable: true,
         bubbles: true,
       });
       await localSearchbox.handleKeyNavigation(tab2);
       await microtasksFinished();

       assertTrue(tab2.defaultPrevented);
       assertEquals(-1, localSearchbox.selection.line);
       assertEquals(
           SelectionLineState.kFocusedButtonContextEntrypoint,
           localSearchbox.selection.state);
       assertTrue(localSearchbox.isContextEntrypointVirtualFocused());
       assertTrue(entrypointButton.hasVirtualFocus);

       // 3rd Forward Tab: wraps forward from contextual entrypoint to match 0.
       const tab3 = new KeyboardEvent('keydown', {
         key: 'Tab',
         shiftKey: false,
         cancelable: true,
         bubbles: true,
       });
       await localSearchbox.handleKeyNavigation(tab3);
       await microtasksFinished();

       assertTrue(tab3.defaultPrevented);
       assertEquals(0, localSearchbox.selection.line);
       assertEquals(SelectionLineState.kNormal, localSearchbox.selection.state);
       assertFalse(localSearchbox.isContextEntrypointVirtualFocused());
       assertFalse(entrypointButton.hasVirtualFocus);
     });

 test('ShiftTabAndTabWithVirtualFocusCyclesThroughLensSearchIcon', async () => {
   document.body.innerHTML = window.trustedTypes!.emptyHTML;
   loadTimeData.overrideValues({
     realboxVirtualFocusNavigation: true,
     hideClassicContextButton: false,
     contextualMenuUsePecApi: false,
     searchboxLayoutMode: 'TallBottomContext',
     composeboxShowChip: false,
     composeboxShowLensIcon: true,
   });
   const localSearchbox = document.createElement('omnibox-popup-searchbox');
   localSearchbox.dropdownIsVisible = true;
   localSearchbox.virtualFocusEnabled = true;
   document.body.appendChild(localSearchbox);
   await microtasksFinished();

   testProxy.initVisibilityPrefs();
   testProxy.page.updateAimPopupEligibility(true);
   testProxy.page.updateLensSearchEligibility(true);
   await microtasksFinished();

   localSearchbox.activeQueryId = 0;
   localSearchbox.onAutocompleteResultChanged(
       createAutocompleteResultForTesting({
         queryId: 0,
         input: 'test',
         matches: [createSearchMatchForTesting({
           allowedToBeDefaultMatch: true,
           suggestTemplate: createSuggestTemplateInfo({primaryText: 'match 1'}),
         })],
       }));
   await microtasksFinished();

   assertTrue(localSearchbox.showContextEntrypoint);
   assertTrue(localSearchbox.showLensSearchIcon);
   const entrypointButton = getContextualEntrypointButton(localSearchbox);
   const lensSearchIcon = getLensSearchIcon(localSearchbox);
   assertTrue(!!entrypointButton);
   assertTrue(!!lensSearchIcon);

   localSearchbox.setSelection({
     line: 0,
     state: SelectionLineState.kNormal,
     actionIndex: 0,
   });
   await microtasksFinished();

   const pressTab = async (shiftKey: boolean) => {
     await localSearchbox.handleKeyNavigation(new KeyboardEvent(
         'keydown', {key: 'Tab', shiftKey, cancelable: true, bubbles: true}));
     await microtasksFinished();
   };

   // 1st Shift-Tab: cycles backward from match 0 to lens search icon.
   await pressTab(/*shiftKey=*/ true);
   assertEquals(
       SelectionLineState.kFocusedButtonLensSearch,
       localSearchbox.selection.state);
   assertTrue(lensSearchIcon.hasVirtualFocus);
   assertFalse(entrypointButton.hasVirtualFocus);

   // 2nd Shift-Tab: moves backward from lens search icon to contextual entrypoint.
   await pressTab(/*shiftKey=*/ true);
   assertEquals(
       SelectionLineState.kFocusedButtonContextEntrypoint,
       localSearchbox.selection.state);
   assertFalse(lensSearchIcon.hasVirtualFocus);
   assertTrue(entrypointButton.hasVirtualFocus);

   // Forward Tab: moves forward from contextual entrypoint to lens search icon.
   await pressTab(/*shiftKey=*/ false);
   assertEquals(
       SelectionLineState.kFocusedButtonLensSearch,
       localSearchbox.selection.state);
   assertTrue(lensSearchIcon.hasVirtualFocus);
   assertFalse(entrypointButton.hasVirtualFocus);

   // 2nd Forward Tab: wraps forward from lens search icon to match 0.
   await pressTab(/*shiftKey=*/ false);
   assertEquals(0, localSearchbox.selection.line);
   assertEquals(SelectionLineState.kNormal, localSearchbox.selection.state);
   assertFalse(lensSearchIcon.hasVirtualFocus);
 });

 test('ShiftTabAndTabWithVirtualFocusCyclesThroughLensSearchChip', async () => {
   document.body.innerHTML = window.trustedTypes!.emptyHTML;
   loadTimeData.overrideValues({
     realboxVirtualFocusNavigation: true,
     hideClassicContextButton: false,
     contextualMenuUsePecApi: false,
     searchboxLayoutMode: 'TallBottomContext',
     composeboxShowChip: true,
     composeboxShowLensIcon: false,
   });
   const localSearchbox = document.createElement('omnibox-popup-searchbox');
   localSearchbox.dropdownIsVisible = true;
   localSearchbox.virtualFocusEnabled = true;
   document.body.appendChild(localSearchbox);
   await microtasksFinished();

   testProxy.initVisibilityPrefs();
   testProxy.page.updateAimPopupEligibility(true);
   testProxy.page.updateLensSearchEligibility(true);
   await microtasksFinished();

   localSearchbox.activeQueryId = 0;
   localSearchbox.onAutocompleteResultChanged(
       createAutocompleteResultForTesting({
         queryId: 0,
         input: 'test',
         matches: [createSearchMatchForTesting({
           allowedToBeDefaultMatch: true,
           suggestTemplate: createSuggestTemplateInfo({primaryText: 'match 1'}),
         })],
       }));
   await microtasksFinished();

   assertTrue(localSearchbox.showContextEntrypoint);
   assertTrue(localSearchbox.showContextualChip);
   assertFalse(localSearchbox.showLensSearchIcon);
   const entrypointButton = getContextualEntrypointButton(localSearchbox);
   const lensSearchChip = getLensSearchChip(localSearchbox);
   assertTrue(!!entrypointButton);
   assertTrue(!!lensSearchChip);

   localSearchbox.setSelection({
     line: 0,
     state: SelectionLineState.kNormal,
     actionIndex: 0,
   });
   await microtasksFinished();

   const pressTab = async (shiftKey: boolean) => {
     await localSearchbox.handleKeyNavigation(new KeyboardEvent(
         'keydown', {key: 'Tab', shiftKey, cancelable: true, bubbles: true}));
     await microtasksFinished();
   };

   // 1st Shift-Tab: cycles backward from match 0 to lens search chip.
   await pressTab(/*shiftKey=*/ true);
   assertEquals(
       SelectionLineState.kFocusedButtonContextualChip,
       localSearchbox.selection.state);
   assertTrue(lensSearchChip.hasVirtualFocus);
   assertFalse(entrypointButton.hasVirtualFocus);

   // 2nd Shift-Tab: moves backward from lens search chip to contextual entrypoint.
   await pressTab(/*shiftKey=*/ true);
   assertEquals(
       SelectionLineState.kFocusedButtonContextEntrypoint,
       localSearchbox.selection.state);
   assertFalse(lensSearchChip.hasVirtualFocus);
   assertTrue(entrypointButton.hasVirtualFocus);

   // Forward Tab: moves forward from contextual entrypoint to lens search chip.
   await pressTab(/*shiftKey=*/ false);
   assertEquals(
       SelectionLineState.kFocusedButtonContextualChip,
       localSearchbox.selection.state);
   assertTrue(lensSearchChip.hasVirtualFocus);
   assertFalse(entrypointButton.hasVirtualFocus);

   // 2nd Forward Tab: wraps forward from lens search chip to match 0.
   await pressTab(/*shiftKey=*/ false);
   assertEquals(0, localSearchbox.selection.line);
   assertEquals(SelectionLineState.kNormal, localSearchbox.selection.state);
   assertFalse(lensSearchChip.hasVirtualFocus);
 });

 test(
     'ShiftTabAndTabWithVirtualFocusCyclesThroughCurrentTabChipAndLensIcon',
     async () => {
       document.body.innerHTML = window.trustedTypes!.emptyHTML;
       loadTimeData.overrideValues({
         realboxVirtualFocusNavigation: true,
         hideClassicContextButton: false,
         contextualMenuUsePecApi: false,
         searchboxLayoutMode: 'TallBottomContext',
         composeboxShowChip: true,
         composeboxShowCurrentTabChip: true,
         composeboxShowLensIcon: true,
       });
       testProxy.handler.setPromiseResolveFor<'getRecentTabs'>('getRecentTabs', {
         tabs: [{
           tabId: 1,
           title: 'Current Tab',
           url: 'https://example.com',
           showInCurrentTabChip: true,
           showInPreviousTabChip: false,
           lastActive: {internalValue: 100n},
         }],
       });

       const localSearchbox = document.createElement('omnibox-popup-searchbox');
       localSearchbox.dropdownIsVisible = true;
       localSearchbox.virtualFocusEnabled = true;
       document.body.appendChild(localSearchbox);
       await microtasksFinished();

       testProxy.initVisibilityPrefs();
       testProxy.page.updateAimPopupEligibility(true);
       testProxy.page.updateLensSearchEligibility(true);
       callbackRouter.onShow();
       await testProxy.handler.whenCalled('getRecentTabs');
       await microtasksFinished();

       localSearchbox.activeQueryId = 0;
       localSearchbox.onAutocompleteResultChanged(
           createAutocompleteResultForTesting({
             queryId: 0,
             input: 'test',
             matches: [createSearchMatchForTesting({
               allowedToBeDefaultMatch: true,
               suggestTemplate:
                   createSuggestTemplateInfo({primaryText: 'match 1'}),
             })],
           }));
       await microtasksFinished();

       assertTrue(localSearchbox.showContextEntrypoint);
       assertTrue(localSearchbox.showContextualChip);
       assertTrue(localSearchbox.showLensSearchIcon);
       const entrypointButton = getContextualEntrypointButton(localSearchbox);
       const currentTabChip = getCurrentTabChip(localSearchbox);
       const lensSearchIcon = getLensSearchIcon(localSearchbox);
       assertTrue(!!entrypointButton);
       assertTrue(!!currentTabChip);
       assertTrue(!!lensSearchIcon);

       localSearchbox.setSelection({
         line: 0,
         state: SelectionLineState.kNormal,
         actionIndex: 0,
       });
       await microtasksFinished();

       const pressTab = async (shiftKey: boolean) => {
         await localSearchbox.handleKeyNavigation(new KeyboardEvent(
             'keydown', {key: 'Tab', shiftKey, cancelable: true, bubbles: true}));
         await microtasksFinished();
       };

       // 1st Shift-Tab: match 0 -> lens search icon.
       await pressTab(/*shiftKey=*/ true);
       assertEquals(
           SelectionLineState.kFocusedButtonLensSearch,
           localSearchbox.selection.state);
       assertTrue(lensSearchIcon.hasVirtualFocus);
       assertFalse(currentTabChip.hasVirtualFocus);
       assertFalse(entrypointButton.hasVirtualFocus);

       // 2nd Shift-Tab: lens search icon -> current tab chip.
       await pressTab(/*shiftKey=*/ true);
       assertEquals(
           SelectionLineState.kFocusedButtonContextualChip,
           localSearchbox.selection.state);
       assertFalse(lensSearchIcon.hasVirtualFocus);
       assertTrue(currentTabChip.hasVirtualFocus);
       assertFalse(entrypointButton.hasVirtualFocus);

       // 3rd Shift-Tab: current tab chip -> contextual entrypoint.
       await pressTab(/*shiftKey=*/ true);
       assertEquals(
           SelectionLineState.kFocusedButtonContextEntrypoint,
           localSearchbox.selection.state);
       assertFalse(lensSearchIcon.hasVirtualFocus);
       assertFalse(currentTabChip.hasVirtualFocus);
       assertTrue(entrypointButton.hasVirtualFocus);

       // Forward Tab: contextual entrypoint -> current tab chip -> lens search icon -> match 0.
       await pressTab(/*shiftKey=*/ false);
       assertEquals(
           SelectionLineState.kFocusedButtonContextualChip,
           localSearchbox.selection.state);
       assertTrue(currentTabChip.hasVirtualFocus);

       await pressTab(/*shiftKey=*/ false);
       assertEquals(
           SelectionLineState.kFocusedButtonLensSearch,
           localSearchbox.selection.state);
       assertFalse(currentTabChip.hasVirtualFocus);
       assertTrue(lensSearchIcon.hasVirtualFocus);

       await pressTab(/*shiftKey=*/ false);
       assertEquals(0, localSearchbox.selection.line);
       assertEquals(SelectionLineState.kNormal, localSearchbox.selection.state);
       assertFalse(lensSearchIcon.hasVirtualFocus);
     });

 test('TabKeyFromInputFocusesAimButtonWhenVisible', async () => {
   searchbox.dropdownIsVisible = false;
   searchbox.focusInput();
   testProxy.page.setAimButtonVisible(true);
   await microtasksFinished();

   assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
   handler.reset();

   const tabEvent = new KeyboardEvent('keydown', {
     key: 'Tab',
     cancelable: true,
     bubbles: true,
   });
   await searchbox.handleKeyNavigation(tabEvent);
   await microtasksFinished();

   assertTrue(tabEvent.defaultPrevented);
   assertFalse(searchbox.isAiModeVirtualFocused());
   assertEquals(-1, searchbox.selection.line);
   assertEquals(SelectionLineState.kNormal, searchbox.selection.state);
   assertEquals(searchbox.$.composeButton, searchbox.shadowRoot.activeElement);
   assertEquals(0, handler.getCallCount('advanceFocus'));
 });

 test('TabKeyFromInputAdvancesFocusForwardWhenAimButtonHidden', async () => {
   searchbox.dropdownIsVisible = false;
   searchbox.focusInput();
   testProxy.page.setAimButtonVisible(false);
   await microtasksFinished();

   assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
   handler.reset();

   const tabEvent = new KeyboardEvent('keydown', {
     key: 'Tab',
     cancelable: true,
     bubbles: true,
   });
   await searchbox.handleKeyNavigation(tabEvent);
   await microtasksFinished();

   assertTrue(tabEvent.defaultPrevented);
   assertEquals(1, handler.getCallCount('advanceFocus'));
   assertFalse(handler.getArgs('advanceFocus')[0]);
 });

 test('ShiftTabKeyFromInputAdvancesFocusBackward', async () => {
   searchbox.dropdownIsVisible = false;
   searchbox.focusInput();
   await microtasksFinished();

   assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
   handler.reset();

   const shiftTabEvent = new KeyboardEvent('keydown', {
     key: 'Tab',
     shiftKey: true,
     cancelable: true,
     bubbles: true,
   });
   await searchbox.handleKeyNavigation(shiftTabEvent);
   await microtasksFinished();

   assertTrue(shiftTabEvent.defaultPrevented);
   assertEquals(1, handler.getCallCount('advanceFocus'));
   assertTrue(handler.getArgs('advanceFocus')[0]);
 });

 test('TabKeyFromAimButtonAdvancesFocusForward', async () => {
   searchbox.dropdownIsVisible = false;
   testProxy.page.setAimButtonVisible(true);
   await microtasksFinished();

   searchbox.$.composeButton.focus();
   await microtasksFinished();

   assertEquals(searchbox.$.composeButton, searchbox.shadowRoot.activeElement);
   assertFalse(searchbox.isAiModeVirtualFocused());
   handler.reset();

   const tabEvent = new KeyboardEvent('keydown', {
     key: 'Tab',
     cancelable: true,
     bubbles: true,
   });
   await searchbox.handleKeyNavigation(tabEvent);
   await microtasksFinished();

   assertTrue(tabEvent.defaultPrevented);
   assertEquals(-1, searchbox.selection.line);
   assertEquals(SelectionLineState.kNormal, searchbox.selection.state);
   assertFalse(searchbox.isAiModeVirtualFocused());
   assertEquals(1, handler.getCallCount('advanceFocus'));
   assertFalse(handler.getArgs('advanceFocus')[0]);
 });

 test('ShiftTabKeyFromAimButtonReturnsFocusToInput', async () => {
   searchbox.dropdownIsVisible = false;
   testProxy.page.setAimButtonVisible(true);
   await microtasksFinished();

   searchbox.$.composeButton.focus();
   await microtasksFinished();

   assertEquals(searchbox.$.composeButton, searchbox.shadowRoot.activeElement);
   assertFalse(searchbox.isAiModeVirtualFocused());
   handler.reset();

   const shiftTabEvent = new KeyboardEvent('keydown', {
     key: 'Tab',
     shiftKey: true,
     cancelable: true,
     bubbles: true,
   });
   await searchbox.handleKeyNavigation(shiftTabEvent);
   await microtasksFinished();

   assertTrue(shiftTabEvent.defaultPrevented);
   assertEquals(-1, searchbox.selection.line);
   assertEquals(SelectionLineState.kNormal, searchbox.selection.state);
   assertFalse(searchbox.isAiModeVirtualFocused());
   assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
   assertEquals(0, handler.getCallCount('advanceFocus'));
 });

 test(
     'TabKeyWithDropdownVisibleDelegatesToSuperWhenVirtualFocusEnabled',
     async () => {
       loadTimeData.overrideValues({realboxVirtualFocusNavigation: true});
       searchbox.virtualFocusEnabled = true;
       searchbox.dropdownIsVisible = true;

       const match = createSearchMatchForTesting({
         allowedToBeDefaultMatch: true,
         suggestTemplate:
             createSuggestTemplateInfo({primaryText: 'first match'}),
       });
       searchbox.activeQueryId = 0;
       searchbox.onAutocompleteResultChanged(
           createAutocompleteResultForTesting({
             queryId: 0,
             input: 'first',
             matches: [match],
           }));
       await microtasksFinished();

       searchbox.focusInput();
       handler.reset();

       const tabEvent = new KeyboardEvent('keydown', {
         key: 'Tab',
         cancelable: true,
         bubbles: true,
       });
       await searchbox.handleKeyNavigation(tabEvent);
       await microtasksFinished();

       // Verify focus did not advance out of the omnibox popup.
       assertEquals(0, handler.getCallCount('advanceFocus'));
     });

 test(
     'TabKeyIgnoredWhenNeitherInputNorAimButtonFocusedAndDropdownClosed',
     async () => {
       searchbox.dropdownIsVisible = false;
       searchbox.blur();
       searchbox.setSelection(kDefaultSelection);
       searchbox.$.input.inputElement.blur();
       await microtasksFinished();

       handler.reset();

       const tabEvent = new KeyboardEvent('keydown', {
         key: 'Tab',
         cancelable: true,
         bubbles: true,
       });
       await searchbox.handleKeyNavigation(tabEvent);
       await microtasksFinished();

       assertFalse(tabEvent.defaultPrevented);
       assertEquals(0, handler.getCallCount('advanceFocus'));
     });

 test('CtrlTabAndOptionCmdTabIgnoredToAllowBrowserTabSwitching', async () => {
   searchbox.dropdownIsVisible = false;
   searchbox.focusInput();
   testProxy.page.setAimButtonVisible(true);
   await microtasksFinished();

   assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
   handler.reset();

   // 1. Ctrl+Tab does not advance focus to AIM button or out of omnibox.
   const ctrlTabEvent = new KeyboardEvent('keydown', {
     key: 'Tab',
     ctrlKey: true,
     cancelable: true,
     bubbles: true,
   });
   await searchbox.handleKeyNavigation(ctrlTabEvent);
   await microtasksFinished();

   assertFalse(ctrlTabEvent.defaultPrevented);
   assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
   assertEquals(0, handler.getCallCount('advanceFocus'));

   // 2. Ctrl+Shift+Tab does not advance focus backwards out of omnibox.
   const ctrlShiftTabEvent = new KeyboardEvent('keydown', {
     key: 'Tab',
     ctrlKey: true,
     shiftKey: true,
     cancelable: true,
     bubbles: true,
   });
   await searchbox.handleKeyNavigation(ctrlShiftTabEvent);
   await microtasksFinished();

   assertFalse(ctrlShiftTabEvent.defaultPrevented);
   assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
   assertEquals(0, handler.getCallCount('advanceFocus'));

   // 3. Option+Cmd+Tab (altKey && metaKey on macOS) does not advance focus.
   const altMetaTabEvent = new KeyboardEvent('keydown', {
     key: 'Tab',
     altKey: true,
     metaKey: true,
     cancelable: true,
     bubbles: true,
   });
   await searchbox.handleKeyNavigation(altMetaTabEvent);
   await microtasksFinished();

   assertFalse(altMetaTabEvent.defaultPrevented);
   assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
   assertEquals(0, handler.getCallCount('advanceFocus'));
 });

 test('CtrlTabIgnoredWithDropdownVisible', async () => {
   searchbox.dropdownIsVisible = true;
   searchbox.focusInput();

   const match = createSearchMatchForTesting({
     allowedToBeDefaultMatch: true,
     suggestTemplate: createSuggestTemplateInfo({primaryText: 'first match'}),
     inlineAutocompletion: ' match',
   });
   searchbox.activeQueryId = 0;
   searchbox.onAutocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: 0,
     input: 'first',
     matches: [match],
   }));
   await microtasksFinished();

   const initialSelection = searchbox.selection;

   const ctrlTabEvent = new KeyboardEvent('keydown', {
     key: 'Tab',
     ctrlKey: true,
     cancelable: true,
     bubbles: true,
   });
   await searchbox.handleKeyNavigation(ctrlTabEvent);
   await microtasksFinished();

   assertFalse(ctrlTabEvent.defaultPrevented);
   assertEquals(initialSelection.line, searchbox.selection.line);
   assertEquals(initialSelection.state, searchbox.selection.state);
 });

 test('UnfocusedSingleClick_SelectsAllOnMouseUp', async () => {
   const testText = 'https://example.com/test';
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: testText,
     isFocused: false,
     selection: {start: 0, end: 0},
   }));
   await microtasksFinished();

   const input = searchbox.getInputElement().inputElement;
   assertEquals(0, input.selectionStart);
   assertEquals(0, input.selectionEnd);

   // Mousedown on unfocused input.
   searchbox.getInputElement().dispatchEvent(new MouseEvent('mousedown', {
     button: 0,
     buttons: 1,
     clientX: 20,
     clientY: 20,
     detail: 1,
     bubbles: true,
     composed: true,
   }));
   input.focus();
   await microtasksFinished();

   // Mouseup on document without movement.
   document.dispatchEvent(new MouseEvent('mouseup', {
     button: 0,
     buttons: 0,
     clientX: 20,
     clientY: 20,
     detail: 1,
     bubbles: true,
     composed: true,
   }));
   await microtasksFinished();

   // Verify all text is selected on mouse release.
   assertEquals(0, input.selectionStart);
   assertEquals(testText.length, input.selectionEnd);
 });

 test('UnfocusedDragSelection_PreservesPartialSelection', async () => {
   const testText = 'https://example.com/test';
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: testText,
     isFocused: false,
     selection: {start: 0, end: 0},
   }));
   await microtasksFinished();

   const input = searchbox.getInputElement().inputElement;

   // Mousedown on unfocused input.
   searchbox.getInputElement().dispatchEvent(new MouseEvent('mousedown', {
     button: 0,
     buttons: 1,
     clientX: 20,
     clientY: 20,
     detail: 1,
     bubbles: true,
     composed: true,
   }));
   input.focus();

   // Simulate Blink setting partial drag selection.
   input.setSelectionRange(8, 19);

   // Mousemove beyond 3px threshold.
   document.dispatchEvent(new MouseEvent('mousemove', {
     buttons: 1,
     clientX: 80,
     clientY: 20,
     bubbles: true,
     composed: true,
   }));

   // Mouseup on document.
   document.dispatchEvent(new MouseEvent('mouseup', {
     button: 0,
     buttons: 0,
     clientX: 80,
     clientY: 20,
     detail: 1,
     bubbles: true,
     composed: true,
   }));
   await microtasksFinished();

   // Verify partial drag selection was preserved and NOT overwritten by
   // select-all.
   assertEquals(8, input.selectionStart);
   assertEquals(19, input.selectionEnd);
   assertEquals('example.com', input.value.substring(8, 19));
 });

 test('DoubleClick_PreservesWordSelection', async () => {
   const testText = 'hello world';
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: testText,
     isFocused: false,
     selection: {start: 0, end: 0},
   }));
   await microtasksFinished();

   const input = searchbox.getInputElement().inputElement;

   // Simulate click 1 (select all on mouseup).
   input.dispatchEvent(new MouseEvent('mousedown', {
     button: 0,
     buttons: 1,
     clientX: 10,
     clientY: 10,
     detail: 1,
     bubbles: true,
     composed: true,
   }));
   input.focus();
   document.dispatchEvent(new MouseEvent('mouseup', {
     button: 0,
     buttons: 0,
     clientX: 10,
     clientY: 10,
     detail: 1,
     bubbles: true,
     composed: true,
   }));
   await microtasksFinished();
   assertEquals(0, input.selectionStart);
   assertEquals(testText.length, input.selectionEnd);

   // Simulate click 2 (double-click word selection of 'hello').
   input.dispatchEvent(new MouseEvent('mousedown', {
     button: 0,
     buttons: 1,
     clientX: 10,
     clientY: 10,
     detail: 2,
     bubbles: true,
     composed: true,
   }));
   // Mousedown should have collapsed the range selection so Blink can perform
   // native word selection.
   assertEquals(0, input.selectionStart);
   assertEquals(0, input.selectionEnd);

   input.setSelectionRange(0, 5);
   document.dispatchEvent(new MouseEvent('mouseup', {
     button: 0,
     buttons: 0,
     clientX: 10,
     clientY: 10,
     detail: 2,
     bubbles: true,
     composed: true,
   }));
   await microtasksFinished();

   assertEquals(0, input.selectionStart);
   assertEquals(5, input.selectionEnd);
 });

 test('AlreadyFocusedClick_PlacesCaretWithoutSelectAll', async () => {
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: 'hello world',
     isFocused: true,
     selection: {start: 0, end: 11},
   }));
   await microtasksFinished();

   const input = searchbox.getInputElement().inputElement;
   input.focus();
   input.setSelectionRange(5, 5);

   // Click inside already-focused input.
   searchbox.getInputElement().dispatchEvent(new MouseEvent('mousedown', {
     button: 0,
     buttons: 1,
     clientX: 50,
     clientY: 20,
     detail: 1,
     bubbles: true,
     composed: true,
   }));
   document.dispatchEvent(new MouseEvent('mouseup', {
     button: 0,
     buttons: 0,
     clientX: 50,
     clientY: 20,
     detail: 1,
     bubbles: true,
     composed: true,
   }));
   await microtasksFinished();

   assertEquals(5, input.selectionStart);
   assertEquals(5, input.selectionEnd);
 });

 test(
     'RightClickOnAllSelectedText_DoesNotUnelideOrModifySelection',
     async () => {
       const elidedText = 'example.com';
       const fullUrl = 'https://example.com/';
       callbackRouter.setInputState(createDefaultOmniboxInputState({
         text: elidedText,
         fullUrl: fullUrl,
         isFocused: true,
         selection: {start: 0, end: elidedText.length},
       }));
       await microtasksFinished();

       const input = searchbox.getInputElement().inputElement;
       assertEquals(elidedText, input.value);
       assertEquals(0, input.selectionStart);
       assertEquals(elidedText.length, input.selectionEnd);

       // Right-click mousedown on already-focused input with all text selected.
       searchbox.getInputElement().dispatchEvent(new MouseEvent('mousedown', {
         button: 2,
         buttons: 2,
         clientX: 20,
         clientY: 20,
         detail: 1,
         bubbles: true,
         composed: true,
       }));
       await microtasksFinished();

       // Verify text did not unelide and selection was not modified/collapsed.
       assertEquals(elidedText, input.value);
       assertEquals(0, input.selectionStart);
       assertEquals(elidedText.length, input.selectionEnd);
     });

 test('RightClickWhenBlurred_FocusesAndSelectsAll', async () => {
   const text = 'hello world';
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: text,
     isFocused: true,
     selection: {start: text.length, end: text.length},
   }));
   await microtasksFinished();

   const input = searchbox.getInputElement().inputElement;
   assertEquals(text, input.value);
   assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);

   // Blur the searchbox while keeping the popup open (e.g. draft text).
   callbackRouter.setFocus(false, false, false, /*showFullUrl=*/ false);
   await microtasksFinished();
   assertEquals(null, searchbox.shadowRoot.activeElement);

   // Right-click mousedown on the blurred input.
   const event = new MouseEvent('mousedown', {
     button: 2,
     buttons: 2,
     clientX: 20,
     clientY: 20,
     detail: 1,
     bubbles: true,
     cancelable: true,
     composed: true,
   });
   searchbox.getInputElement().dispatchEvent(event);
   await microtasksFinished();

   // Verify the input is focused, all text is selected, and default mousedown
   // was prevented.
   assertTrue(event.defaultPrevented);
   assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
   assertEquals(0, input.selectionStart);
   assertEquals(text.length, input.selectionEnd);
 });

 // Returns points at the vertical center of the input, across the 18px at the
 // end of the input row that are left empty while the AIM button is hidden.
 function getTrailingGapPoints(): Array<{x: number, y: number}> {
   const row = $$<HTMLElement>(searchbox.$.input, '#inputInnerContainer');
   assertTrue(!!row);
   const rowRect = row.getBoundingClientRect();
   const inputRect = searchbox.$.input.inputElement.getBoundingClientRect();
   const y = inputRect.top + inputRect.height / 2;
   const isRtl = getComputedStyle(row).direction === 'rtl';
   const points = [];
   for (let i = 0; i < 18; i++) {
     points.push(
         {x: isRtl ? rowRect.left + i + 0.5 : rowRect.right - i - 0.5, y});
   }
   return points;
 }

 suite('SecondClickOnAllSelectedElidedUrl', () => {
   const elidedText = 'example.com/path';
   const fullUrl = 'https://www.example.com/path';
   const prefixLength = 'https://www.'.length;

   let input: HTMLInputElement|HTMLTextAreaElement;

   setup(async () => {
     callbackRouter.setInputState(createDefaultOmniboxInputState({
       text: elidedText,
       fullUrl: fullUrl,
       isFocused: true,
       selection: {start: 0, end: elidedText.length},
     }));
     await microtasksFinished();
     input = searchbox.getInputElement().inputElement;
     input.focus();
     assertEquals(elidedText, input.value);
     assertEquals(0, input.selectionStart);
     assertEquals(elidedText.length, input.selectionEnd);
   });

   function dispatchMouseEvent(
       target: EventTarget, type: string, clientX: number, detail = 1) {
     const rect = input.getBoundingClientRect();
     target.dispatchEvent(new MouseEvent(type, {
       button: 0,
       buttons: type === 'mousedown' ? 1 : 0,
       clientX,
       clientY: rect.top + rect.height / 2,
       detail,
       bubbles: true,
       composed: true,
     }));
   }

   // Returns what a click on the icon lands on, and the click's client X. The
   // icon has `pointer-events: none`, so a click on it lands on the row behind
   // it. The row isn't focusable, so `OmniboxFullAppElement` prevents the
   // press's default action, and neither the caret nor the focus moves.
   function getRowBehindIcon(): {target: Element, x: number} {
     const iconRect = searchbox.$.input.$.icon.getBoundingClientRect();
     const x = iconRect.left + iconRect.width / 2;
     const target = searchbox.$.input.shadowRoot.elementFromPoint(
         x, iconRect.top + iconRect.height / 2);
     assertTrue(!!target);
     assertTrue(searchbox.$.input.shadowRoot.contains(target));
     assertFalse(target === input);
     return {target, x};
   }

   // Returns a client X coordinate over the input at which a click places the
   // caret at `offset` of the current input text.
   function clientXForOffset(offset: number): number {
     const rect = input.getBoundingClientRect();
     const y = rect.top + rect.height / 2;
     for (let x = Math.ceil(rect.left); x < rect.right; x++) {
       const position = document.caretPositionFromPoint(
           x, y, {shadowRoots: [input.getRootNode() as ShadowRoot]});
       if (position?.offsetNode === input && position.offset === offset) {
         return x;
       }
     }
     assertNotReached(`No client X found for offset ${offset}`);
   }

   test('UnelidesOnMouseUpPlacingCaretOnClickedCharacter', async () => {
     // Click between 'exam' and 'ple' of the elided URL.
     const clientX = clientXForOffset(4);
     dispatchMouseEvent(input, 'mousedown', clientX);
     // Like Views, the press neither unelides nor changes the selection, so
     // that it can still start a drag-and-drop of the selected URL.
     assertEquals(elidedText, input.value);
     assertEquals(0, input.selectionStart);
     assertEquals(elidedText.length, input.selectionEnd);

     dispatchMouseEvent(document, 'mouseup', clientX);
     await microtasksFinished();

     // On release, the caret is placed at the clicked character and the URL is
     // unelided, keeping the caret on the same characters.
     assertEquals(fullUrl, input.value);
     assertEquals(prefixLength + 4, input.selectionStart);
     assertEquals(prefixLength + 4, input.selectionEnd);
     assertEquals(
         'https://www.exam', input.value.substring(0, prefixLength + 4));
   });

   test('UnelidesOnMouseUpOutsideInputPlacingCaretAtEnd', async () => {
     dispatchMouseEvent(input, 'mousedown', clientXForOffset(4));
     // Release to the right of the input, which is clamped to the input, past
     // the end of the elided URL.
     dispatchMouseEvent(
         document, 'mouseup', input.getBoundingClientRect().right + 50);
     await microtasksFinished();

     assertEquals(fullUrl, input.value);
     assertEquals(prefixLength + elidedText.length, input.selectionStart);
     assertEquals(prefixLength + elidedText.length, input.selectionEnd);
   });

   test('UnelidesOnMouseUpShiftingChangedSelection', async () => {
     dispatchMouseEvent(input, 'mousedown', clientXForOffset(4));
     // Simulate the selection having changed to 'example.com' by release.
     input.setSelectionRange(0, 'example.com'.length);
     dispatchMouseEvent(document, 'mouseup', clientXForOffset(4));
     await microtasksFinished();

     // The selection stays on the same characters of the unelided URL, without
     // being extended over the elided prefix.
     assertEquals(fullUrl, input.value);
     assertEquals(prefixLength, input.selectionStart);
     assertEquals(prefixLength + 'example.com'.length, input.selectionEnd);
   });

   test('DoesNotUnelideWhenDraggingSelectedUrl', async () => {
     const clientX = clientXForOffset(4);
     dispatchMouseEvent(input, 'mousedown', clientX);
     // Dragging from inside the selection starts a drag-and-drop of the whole
     // selected URL, rather than a new drag selection.
     input.dispatchEvent(
         new DragEvent('dragstart', {bubbles: true, composed: true}));
     // No `mouseup` follows a real drag (Blink only dispatches `dragend`).
     // Simulate a later, unrelated `mouseup` (e.g. from clicking a suggestion)
     // to verify that the drag cancelled the pending unelision.
     dispatchMouseEvent(document, 'mouseup', clientX);
     await microtasksFinished();

     // Like Views, the URL stays elided and fully selected.
     assertEquals(elidedText, input.value);
     assertEquals(0, input.selectionStart);
     assertEquals(elidedText.length, input.selectionEnd);
   });

   test('UnelidesIntranetUrlWithTrailingSlashElided', async () => {
     // "https://foobar" is elided to "foobar/", which is not a substring of
     // the full URL.
     const intranetElidedText = 'foobar/';
     const intranetFullUrl = 'https://foobar';
     callbackRouter.setInputState(createDefaultOmniboxInputState({
       text: intranetElidedText,
       fullUrl: intranetFullUrl,
       isFocused: true,
       selection: {start: 0, end: intranetElidedText.length},
     }));
     await microtasksFinished();
     input.focus();
     assertEquals(intranetElidedText, input.value);

     // Click between 'foo' and 'bar'.
     const clientX = clientXForOffset(3);
     dispatchMouseEvent(input, 'mousedown', clientX);
     dispatchMouseEvent(document, 'mouseup', clientX);
     await microtasksFinished();

     // The caret stays between 'foo' and 'bar' of the unelided URL.
     assertEquals(intranetFullUrl, input.value);
     assertEquals('https://foo'.length, input.selectionStart);
     assertEquals('https://foo'.length, input.selectionEnd);
   });

   test('UnelidesOnClickInTrailingGapPlacingCaretAtEnd', async () => {
     // The AIM button is hidden by default, leaving a gap at the end of the
     // row. Like in Views, a click in it lands on the input.
     assertFalse(isVisible(searchbox.$.composeButton));
     const {x, y} = getTrailingGapPoints()[9]!;
     const target = searchbox.$.input.shadowRoot.elementFromPoint(x, y);
     assertEquals(input, target);

     dispatchMouseEvent(input, 'mousedown', x);
     dispatchMouseEvent(document, 'mouseup', x);
     await microtasksFinished();

     // The gap is past the end of the text, so the caret ends up at the end of
     // the unelided URL.
     assertEquals(fullUrl, input.value);
     assertEquals(fullUrl.length, input.selectionStart);
     assertEquals(fullUrl.length, input.selectionEnd);
   });

   test('ClickInTrailingGapKeepsCaretInViewForLongUrl', async () => {
     const longElidedText = 'example.com/' +
         'foo/bar/baz/eggs/spam/'.repeat(20);
     const longFullUrl = 'https://www.' + longElidedText;
     callbackRouter.setInputState(createDefaultOmniboxInputState({
       text: longElidedText,
       fullUrl: longFullUrl,
       isFocused: true,
       selection: {start: 0, end: longElidedText.length},
     }));
     await microtasksFinished();
     input.focus();
     assertEquals(longElidedText, input.value);
     assertTrue(input.scrollWidth > input.clientWidth);
     // Like after any select-all, the start of the URL is shown.
     assertEquals(0, input.scrollLeft);

     const {x} = getTrailingGapPoints()[9]!;
     dispatchMouseEvent(input, 'mousedown', x);
     dispatchMouseEvent(document, 'mouseup', x);
     await microtasksFinished();

     // The caret is placed at the last visible character, before the gap, which
     // uneliding shifts by the elided prefix. Like in Views, the input scrolls
     // to keep the caret in view.
     assertEquals(longFullUrl, input.value);
     const caret = input.selectionStart!;
     assertEquals(caret, input.selectionEnd);
     assertTrue(caret > prefixLength, `caret at ${caret}`);
     assertTrue(caret < longFullUrl.length, `caret at ${caret}`);
     assertTextOffsetVisible(input, caret);
     // Scrolled only as far as needed, so the caret sits at the end of the
     // visible text.
     const caretRight = getTextOffsetLeft(input, caret) + 1 - input.scrollLeft;
     assertTrue(
         Math.abs(getVisibleTextWidth(input) - caretRight) <= 1,
         `caret at ${caretRight}px, visible width ${
             getVisibleTextWidth(input)}px`);
   });

   test('ClickBesideInputDoesNotCorruptTextOnNextKeystroke', async () => {
     const {target, x} = getRowBehindIcon();
     dispatchMouseEvent(target, 'mousedown', x);
     dispatchMouseEvent(document, 'mouseup', x);
     await microtasksFinished();

     // The elided URL stays all selected, so that typing replaces all of it,
     // rather than all but the last `prefixLength` characters of the unelided
     // URL.
     assertEquals(elidedText, input.value);
     assertEquals(0, input.selectionStart);
     assertEquals(elidedText.length, input.selectionEnd);
     assertTrue(document.execCommand('insertText', false, 'h'));
     await microtasksFinished();
     assertEquals('h', input.value);
   });

   test('DoubleClickBesideInputKeepsElidedUrlSelected', async () => {
     const {target, x} = getRowBehindIcon();
     dispatchMouseEvent(target, 'mousedown', x);
     dispatchMouseEvent(document, 'mouseup', x);
     dispatchMouseEvent(target, 'mousedown', x, /*detail=*/ 2);
     dispatchMouseEvent(document, 'mouseup', x, /*detail=*/ 2);
     await microtasksFinished();

     // Blink doesn't select a word for a press beside the input, so the
     // selection isn't collapsed for it either. Typing then still replaces all
     // of the elided URL, rather than going in front of it.
     assertEquals(elidedText, input.value);
     assertEquals(0, input.selectionStart);
     assertEquals(elidedText.length, input.selectionEnd);
   });
 });

 test('KeydownDuringMouseDown_CancelsSelectAll', async () => {
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: 'https://example.com',
     isFocused: false,
     selection: {start: 0, end: 0},
   }));
   await microtasksFinished();

   const input = searchbox.getInputElement().inputElement;

   // Mousedown arms tracker.
   searchbox.getInputElement().dispatchEvent(new MouseEvent('mousedown', {
     button: 0,
     buttons: 1,
     clientX: 20,
     clientY: 20,
     detail: 1,
     bubbles: true,
     composed: true,
   }));
   input.focus();

   // Keydown during mousedown.
   searchbox.$.inputWrapper.dispatchEvent(new KeyboardEvent('keydown', {
     key: 'ArrowLeft',
     bubbles: true,
     composed: true,
   }));

   // Mouseup after keydown.
   document.dispatchEvent(new MouseEvent('mouseup', {
     button: 0,
     buttons: 0,
     clientX: 20,
     clientY: 20,
     detail: 1,
     bubbles: true,
     composed: true,
   }));
   await microtasksFinished();

   // Ensure select-all was canceled.
   assertFalse(
       input.selectionStart === 0 &&
       input.selectionEnd === 'https://example.com'.length);
 });

 test('DropdownVisible_LocksOutSelectAllOnMouseDown', async () => {
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: 'test',
     isFocused: false,
     selection: {start: 0, end: 0},
   }));
   testProxy.page.autocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: searchbox.activeQueryId,
     sequenceId: 1,
     input: 'test',
     matches: [createSearchMatchForTesting()],
   }));
   await microtasksFinished();
   assertTrue(searchbox.dropdownIsVisible);

   const input = searchbox.getInputElement().inputElement;

   searchbox.getInputElement().dispatchEvent(new MouseEvent('mousedown', {
     button: 0,
     buttons: 1,
     clientX: 20,
     clientY: 20,
     detail: 1,
     bubbles: true,
     composed: true,
   }));
   input.focus();
   document.dispatchEvent(new MouseEvent('mouseup', {
     button: 0,
     buttons: 0,
     clientX: 20,
     clientY: 20,
     detail: 1,
     bubbles: true,
     composed: true,
   }));
   await microtasksFinished();

   // When dropdown is already visible, mousedown should not force select all.
   assertFalse(
       input.selectionStart === 0 && input.selectionEnd === 'test'.length);
 });

 test('SetsUrlEmphasisFromInputState', async () => {
   const emphasis: UrlEmphasis = {
     deemphasisMode: UrlDeemphasisMode.kAllButHost,
     textIsUrl: true,
     schemeRange: {start: 0, end: 5},
     hostRange: {start: 8, end: 19},
     schemeStrikeThrough: false,
     schemeDangerous: false,
   };
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: 'https://example.com/foo',
     urlEmphasis: emphasis,
   }));
   await microtasksFinished();

   assertDeepEquals(emphasis, searchbox.$.input.urlEmphasis);
 });

 // The #mirror (which renders unselected text with URL emphasis) sits beneath
 // the input (which renders the caret and selected text). Both layers must
 // share the same horizontal box and scroll offset, or else long text that
 // overflows the input ends up drawn twice, misaligned. In RTL UIs, URLs are
 // still laid out LTR (the input and #mirror get dir="ltr"), so these tests
 // also check that the input stays flush with the icon on the leading side.

 // Sets a URL long enough to overflow the input, with the caret at its end
 // (which scrolls the input to its max scroll offset).
 async function setLongUrlWithCaretAtEnd() {
   const text = 'https://example.com/' +
       'foo/bar/baz/eggs/spam/'.repeat(20);
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text,
     urlEmphasis: {
       deemphasisMode: UrlDeemphasisMode.kAllButHost,
       textIsUrl: true,
       schemeRange: {start: 0, end: 5},
       hostRange: {start: 8, end: 19},
       schemeStrikeThrough: false,
       schemeDangerous: false,
     },
     selection: {start: text.length, end: text.length},
     isFocused: true,
   }));
   await microtasksFinished();
 }

 function assertMirrorAlignedWithInput(uiDir: string, step: string) {
   const input = searchbox.$.input.inputElement;
   const mirror = $$<HTMLElement>(searchbox.$.input, '#mirror');
   assertTrue(!!mirror, step);
   assertTrue(isVisible(mirror), step);
   // The UI direction applies to the container, while the URL itself is laid
   // out LTR.
   assertEquals(uiDir, getComputedStyle(mirror.parentElement!).direction, step);
   assertEquals('ltr', getComputedStyle(input).direction, step);
   assertEquals('ltr', getComputedStyle(mirror).direction, step);
   assertTrue(input.scrollLeft > 0, `${step}: input should be scrolled`);

   const iconRect = searchbox.$.input.$.icon.getBoundingClientRect();
   const inputRect = input.getBoundingClientRect();
   const mirrorRect = mirror.getBoundingClientRect();
   assertEquals(
       uiDir === 'ltr' ? iconRect.right : iconRect.left,
       uiDir === 'ltr' ? inputRect.left : inputRect.right,
       `${step}: leading edge`);
   assertEquals(inputRect.left, mirrorRect.left, `${step}: left edge`);
   assertEquals(inputRect.right, mirrorRect.right, `${step}: right edge`);
   // The text areas (inside the padding) and the max scroll offsets must match
   // too. Unlike for #mirror, the input's clientWidth excludes its padding, so
   // compare the max scroll offsets rather than the client widths.
   const inputStyle = getComputedStyle(input);
   const mirrorStyle = getComputedStyle(mirror);
   assertEquals(
       inputStyle.paddingLeft, mirrorStyle.paddingLeft, `${step}: paddingLeft`);
   assertEquals(
       inputStyle.paddingRight, mirrorStyle.paddingRight,
       `${step}: paddingRight`);
   assertEquals(
       input.scrollWidth - input.clientWidth,
       mirror.scrollWidth - mirror.clientWidth, `${step}: max scrollLeft`);
   assertEquals(input.scrollLeft, mirror.scrollLeft, `${step}: scrollLeft`);
 }

 [false, true].forEach(aimButtonVisible => {
   ['ltr', 'rtl'].forEach(uiDir => {
     test(
         `MirrorAlignsWithScrolledInput_AimButton${
             aimButtonVisible ? 'Visible' : 'Hidden'}_${uiDir.toUpperCase()}`,
         async () => {
           // `searchbox` is recreated for every test, so this doesn't leak.
           searchbox.dir = uiDir;
           testProxy.page.setAimButtonVisible(aimButtonVisible);
           await microtasksFinished();

           await setLongUrlWithCaretAtEnd();
           assertEquals(aimButtonVisible, isVisible(searchbox.$.composeButton));

           // The caret is at the end of the overflowing text, so the input is
           // scrolled to (about) its max scroll offset. scrollLeft can be
           // fractional while scrollWidth and clientWidth are rounded.
           const input = searchbox.$.input.inputElement;
           assertTrue(
               input.scrollLeft >= input.scrollWidth - input.clientWidth - 1);
           assertMirrorAlignedWithInput(uiDir, 'caret at end');
         });
   });
 });

 // The AIM button can be shown/hidden while a URL is already displayed (e.g.
 // by toggling "Always show AI Mode" in the omnibox context menu), which
 // resizes the input (and thus may re-clamp its scroll offset) in place.
 ['ltr', 'rtl'].forEach(uiDir => {
   test(
       `MirrorAlignsWithScrolledInputWhenAimButtonToggled_${
           uiDir.toUpperCase()}`,
       async () => {
         searchbox.dir = uiDir;
         testProxy.page.setAimButtonVisible(true);
         await microtasksFinished();
         await setLongUrlWithCaretAtEnd();
         assertTrue(isVisible(searchbox.$.composeButton));
         assertMirrorAlignedWithInput(uiDir, 'initially visible');

         testProxy.page.setAimButtonVisible(false);
         await microtasksFinished();
         assertFalse(isVisible(searchbox.$.composeButton));
         assertMirrorAlignedWithInput(uiDir, 'after hiding');

         testProxy.page.setAimButtonVisible(true);
         await microtasksFinished();
         assertTrue(isVisible(searchbox.$.composeButton));
         assertMirrorAlignedWithInput(uiDir, 'after showing again');
       });
 });

 // The popup stays open, but unfocused, when the user edits the text and then
 // clicks into the page. Like the input (whose own text is transparent), the
 // #mirror must then elide its overflowing end with an ellipsis, at the same
 // scroll offset and within the same box as the input.
 test('MirrorElidesOverflowingTextWhenUnfocused', async () => {
   const text = 'https://example.com/' +
       'foo/bar/baz/eggs/spam/'.repeat(20);
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text,
     urlEmphasis: {
       deemphasisMode: UrlDeemphasisMode.kAllButHost,
       textIsUrl: true,
       schemeRange: {start: 0, end: 5},
       hostRange: {start: 8, end: 19},
       schemeStrikeThrough: false,
       schemeDangerous: false,
     },
     selection: {start: text.length, end: text.length},
     userInputInProgress: true,
     isFocused: true,
   }));
   await microtasksFinished();

   const input = searchbox.$.input.inputElement;
   const mirror = $$<HTMLElement>(searchbox.$.input, '#mirror');
   assertTrue(!!mirror);
   assertTrue(isVisible(mirror));
   assertTrue(input.scrollWidth > input.clientWidth, 'text should overflow');

   // While focused, the text scrolls with the caret, so it must not be elided.
   assertTrue(mirror.parentElement!.matches(':focus-within'));
   assertTrue(input.scrollLeft > 0, 'focused input should be scrolled');
   assertEquals('clip', getComputedStyle(mirror).textOverflow);

   callbackRouter.setFocus(
       false, /*queryZps=*/ false, /*selectAll=*/ false,
       /*showFullUrl=*/ false);
   await microtasksFinished();
   // The input is scrolled back to its start once unfocused (whether or not
   // the page has system focus, without which blur() fires no blur event), and
   // the #mirror follows on the input's scroll event, which fires before the
   // next animation frame callbacks.
   await new Promise(resolve => requestAnimationFrame(resolve));
   assertFalse(mirror.parentElement!.matches(':focus-within'));
   assertFalse(isVisible(searchbox.$.composeButton));
   assertEquals(text, input.value);
   assertEquals(0, input.scrollLeft);
   assertEquals(0, mirror.scrollLeft);
   assertEquals('ellipsis', getComputedStyle(mirror).textOverflow);
   // The ellipsis is styled like the trailing (de-emphasized) path.
   const lastSpan = mirror.lastElementChild!;
   assertTrue(lastSpan.classList.contains('dimmed'));
   assertEquals(
       getComputedStyle(lastSpan).color, getComputedStyle(mirror).color);
   const inputRect = input.getBoundingClientRect();
   const mirrorRect = mirror.getBoundingClientRect();
   assertEquals(inputRect.left, mirrorRect.left);
   assertEquals(inputRect.right, mirrorRect.right);

   // Refocusing stops eliding the text again.
   callbackRouter.setFocus(
       true, /*queryZps=*/ false, /*selectAll=*/ false, /*showFullUrl=*/ false);
   await microtasksFinished();
   assertTrue(mirror.parentElement!.matches(':focus-within'));
   assertEquals('clip', getComputedStyle(mirror).textOverflow);
 });

 // Switching to a tab whose omnibox has an unfocused draft restores it with
 // the caret at its end. The input isn't focused, so it must show the start of
 // the text, with the #mirror eliding the end (rather than scrolling the
 // ellipsis out of view).
 test('MirrorElidesUnfocusedDraftAfterTabSwitch', async () => {
   callbackRouter.setInputState(
       createDefaultOmniboxInputState({text: 'https://example.org/'}));
   await microtasksFinished();
   const input = searchbox.$.input.inputElement;
   const mirror = $$<HTMLElement>(searchbox.$.input, '#mirror');
   assertTrue(!!mirror);
   assertEquals(0, input.scrollLeft);

   // `OnTabChanged()` restores an unfocused draft with the caret at its end.
   const text = 'https://example.com/' +
       'foo/bar/baz/eggs/spam/'.repeat(20);
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text,
     selection: {start: text.length, end: text.length},
     userInputInProgress: true,
     isTabSwitch: true,
   }));
   await microtasksFinished();
   // The #mirror follows on the input's (asynchronous) scroll event.
   await new Promise(resolve => requestAnimationFrame(resolve));
   assertTrue(input.scrollWidth > input.clientWidth, 'text should overflow');
   assertEquals('ellipsis', getComputedStyle(mirror).textOverflow);
   assertEquals(0, input.scrollLeft);
   assertEquals(0, mirror.scrollLeft);
 });

 // While the AIM button is hidden, the gap it leaves at the end of the row is
 // part of the input (with #mirror padded alike), so that a click in it places
 // the caret, like in the Views omnibox. The gap must stay at the end of the
 // field in RTL UIs, even though URLs are laid out LTR.
 ['ltr', 'rtl'].forEach(uiDir => {
   test(
       `TrailingGapIsPartOfInputWhenAimButtonHidden_${uiDir.toUpperCase()}`,
       async () => {
         searchbox.dir = uiDir;
         testProxy.page.setAimButtonVisible(false);
         await microtasksFinished();
         await setLongUrlWithCaretAtEnd();
         assertFalse(isVisible(searchbox.$.composeButton));
         assertMirrorAlignedWithInput(uiDir, 'caret at end');

         const input = searchbox.$.input.inputElement;
         for (const {x, y} of getTrailingGapPoints()) {
           assertEquals(
               input, searchbox.$.input.shadowRoot.elementFromPoint(x, y),
               `hit-test at (${x}, ${y})`);
         }

         // The text still stops short of the gap.
         const mirror = $$<HTMLElement>(searchbox.$.input, '#mirror');
         assertTrue(!!mirror);
         for (const element of [input, mirror]) {
           const style = getComputedStyle(element);
           assertEquals(
               uiDir === 'ltr' ? '0px' : '18px', style.paddingLeft, element.id);
           assertEquals(
               uiDir === 'ltr' ? '18px' : '0px', style.paddingRight,
               element.id);
         }
       });
 });

 test('UpdatesUrlEmphasisFromAutocompleteMatchOnTextInput', async () => {
   const matchEmphasis: UrlEmphasis = {
     deemphasisMode: UrlDeemphasisMode.kAllButHost,
     textIsUrl: true,
     schemeRange: {start: 0, end: 5},
     hostRange: {start: 8, end: 19},
     schemeStrikeThrough: false,
     schemeDangerous: false,
   };

   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: '',
     isFocused: true,
   }));
   await microtasksFinished();

   // Simulate input typing; urlEmphasis is null until autocomplete results
   // arrive.
   searchbox.$.input.setInputText('https://example.com/test');
   searchbox.$.input.fire(
       'searchbox-input-text-updated',
       {value: 'https://example.com/test', isComposing: false});
   await microtasksFinished();
   assertEquals(null, searchbox.$.input.urlEmphasis);

   // Push autocomplete results with precomputed urlEmphasis on default match.
   testProxy.page.autocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: searchbox.activeQueryId,
     input: 'https://example.com/test',
     matches: [
       createSearchMatchForTesting({
         allowedToBeDefaultMatch: true,
         isSearchType: false,
         fillIntoEdit: 'https://example.com/test',
         inlineAutocompletion: '',
         urlEmphasis: matchEmphasis,
       }),
     ],
   }));
   await microtasksFinished();

   assertDeepEquals(matchEmphasis, searchbox.$.input.urlEmphasis);
 });

 test('UpdatesUrlEmphasisOnUnelide', async () => {
   const elidedEmphasis: UrlEmphasis = {
     deemphasisMode: UrlDeemphasisMode.kAllButHost,
     textIsUrl: true,
     schemeRange: {start: 0, end: 0},
     hostRange: {start: 0, end: 11},
     schemeStrikeThrough: false,
     schemeDangerous: false,
   };
   const unelidedEmphasis: UrlEmphasis = {
     deemphasisMode: UrlDeemphasisMode.kAllButHost,
     textIsUrl: true,
     schemeRange: {start: 0, end: 5},
     hostRange: {start: 8, end: 19},
     schemeStrikeThrough: false,
     schemeDangerous: false,
   };

   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: 'example.com',
     fullUrl: 'https://example.com/',
     urlEmphasis: elidedEmphasis,
     unelidedUrlEmphasis: unelidedEmphasis,
     showFullUrl: false,
   }));
   await microtasksFinished();

   assertDeepEquals(elidedEmphasis, searchbox.$.input.urlEmphasis);

   // Unelide URL by selecting a subrange via input state.
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: 'example.com',
     fullUrl: 'https://example.com/',
     urlEmphasis: elidedEmphasis,
     unelidedUrlEmphasis: unelidedEmphasis,
     selection: {start: 0, end: 5},
     showFullUrl: false,
   }));
   await microtasksFinished();

   assertEquals(
       'https://example.com/', searchbox.getInputElement().inputElement.value);
   assertDeepEquals(unelidedEmphasis, searchbox.$.input.urlEmphasis);
 });

 test('SetsSecuritySchemeEmphasisFromInputState', async () => {
   const emphasis: UrlEmphasis = {
     deemphasisMode: UrlDeemphasisMode.kAllButHost,
     textIsUrl: true,
     schemeRange: {start: 0, end: 5},
     hostRange: {start: 8, end: 26},
     schemeStrikeThrough: true,
     schemeDangerous: true,
   };
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: 'https://expired.badssl.com/',
     urlEmphasis: emphasis,
   }));
   await microtasksFinished();

   assertDeepEquals(emphasis, searchbox.$.input.urlEmphasis);
 });

 test(
     'UpdatesUrlEmphasisOnKeyboardArrowNavigationAndEscapeRevert', async () => {
       const steadyStateEmphasis: UrlEmphasis = {
         deemphasisMode: UrlDeemphasisMode.kAllButHost,
         textIsUrl: true,
         schemeRange: {start: 0, end: 0},
         hostRange: {start: 0, end: 11},
         schemeStrikeThrough: false,
         schemeDangerous: false,
       };
       const match0Emphasis: UrlEmphasis = {
         deemphasisMode: UrlDeemphasisMode.kAllButHost,
         textIsUrl: true,
         schemeRange: {start: 0, end: 0},
         hostRange: {start: 0, end: 10},
         schemeStrikeThrough: false,
         schemeDangerous: false,
       };
       const match1Emphasis: UrlEmphasis = {
         deemphasisMode: UrlDeemphasisMode.kAllButHost,
         textIsUrl: true,
         schemeRange: {start: 0, end: 5},
         hostRange: {start: 8, end: 18},
         schemeStrikeThrough: false,
         schemeDangerous: false,
       };

       // Start on a webpage with a steady-state URL.
       callbackRouter.setInputState(createDefaultOmniboxInputState({
         text: 'example.com/page',
         permanentDisplayText: 'example.com/page',
         fullUrl: 'https://example.com/page',
         urlEmphasis: steadyStateEmphasis,
         isFocused: true,
       }));
       await microtasksFinished();
       assertDeepEquals(steadyStateEmphasis, searchbox.$.input.urlEmphasis);

       // User types 'red'.
       searchbox.$.input.setInputText('red');
       searchbox.$.input.fire(
           'searchbox-input-text-updated', {value: 'red', isComposing: false});
       await microtasksFinished();

       // Provide autocomplete results with precomputed urlEmphasis.
       const matches = [
         createSearchMatchForTesting({
           allowedToBeDefaultMatch: true,
           isSearchType: false,
           fillIntoEdit: 'redhat.com',
           inlineAutocompletion: 'hat.com',
           urlEmphasis: match0Emphasis,
         }),
         createSearchMatchForTesting({
           allowedToBeDefaultMatch: false,
           isSearchType: false,
           fillIntoEdit: 'https://redhat.com',
           destinationUrl: 'https://redhat.com',
           urlEmphasis: match1Emphasis,
         }),
       ];
       testProxy.page.autocompleteResultChanged(
           createAutocompleteResultForTesting({
             queryId: searchbox.activeQueryId,
             input: 'red',
             matches: matches,
           }));
       await microtasksFinished();

       assertTrue(searchbox.dropdownIsVisible);
       assertDeepEquals(match0Emphasis, searchbox.$.input.urlEmphasis);

       // Arrow down to second match.
       await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
         bubbles: true,
         cancelable: true,
         key: 'ArrowDown',
       }));
       await microtasksFinished();

       assertEquals(1, searchbox.selectedMatchIndex);
       assertEquals(
           'https://redhat.com',
           searchbox.getInputElement().inputElement.value);
       assertDeepEquals(match1Emphasis, searchbox.$.input.urlEmphasis);

       // Escape Stage 1 (kRevertTemporaryText): reverts to default match 0.
       await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
         bubbles: true,
         cancelable: true,
         key: 'Escape',
       }));
       await microtasksFinished();
       assertEquals(0, searchbox.selectedMatchIndex);
       assertEquals(
           'redhat.com', searchbox.getInputElement().inputElement.value);
       assertDeepEquals(match0Emphasis, searchbox.$.input.urlEmphasis);

       // Escape Stage 2 (kClosePopup): closes dropdown.
       await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
         bubbles: true,
         cancelable: true,
         key: 'Escape',
       }));
       await microtasksFinished();
       assertFalse(searchbox.dropdownIsVisible);

       // Escape Stage 3 (kClearUserInput): reverts to permanentDisplayText and
       // restores steadyStateEmphasis.
       await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
         bubbles: true,
         cancelable: true,
         key: 'Escape',
       }));
       await microtasksFinished();
       assertEquals(
           'example.com/page', searchbox.getInputElement().inputElement.value);
       assertDeepEquals(steadyStateEmphasis, searchbox.$.input.urlEmphasis);
     });

 test('ClearsUrlEmphasisWhenInputEmptied', async () => {
   const emphasis: UrlEmphasis = {
     deemphasisMode: UrlDeemphasisMode.kAllButHost,
     textIsUrl: true,
     schemeRange: {start: 0, end: 5},
     hostRange: {start: 8, end: 18},
     schemeStrikeThrough: false,
     schemeDangerous: false,
   };
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: 'https://redhat.com',
     urlEmphasis: emphasis,
   }));
   await microtasksFinished();
   assertDeepEquals(emphasis, searchbox.$.input.urlEmphasis);

   searchbox.getInputElement().setInput({text: '', inline: ''});
   await microtasksFinished();

   assertEquals(null, searchbox.$.input.urlEmphasis);
 });

 test('RestoresSteadyStateUrlEmphasisForElidedAndUnelidedUrls', async () => {
   const elidedEmphasis: UrlEmphasis = {
     deemphasisMode: UrlDeemphasisMode.kAllButHost,
     textIsUrl: true,
     schemeRange: {start: 0, end: 0},
     hostRange: {start: 0, end: 11},
     schemeStrikeThrough: false,
     schemeDangerous: false,
   };
   const unelidedEmphasis: UrlEmphasis = {
     deemphasisMode: UrlDeemphasisMode.kAllButHost,
     textIsUrl: true,
     schemeRange: {start: 0, end: 5},
     hostRange: {start: 8, end: 19},
     schemeStrikeThrough: false,
     schemeDangerous: false,
   };
   const match0Emphasis: UrlEmphasis = {
     deemphasisMode: UrlDeemphasisMode.kAllButHost,
     textIsUrl: true,
     schemeRange: {start: 0, end: 5},
     hostRange: {start: 8, end: 18},
     schemeStrikeThrough: false,
     schemeDangerous: false,
   };
   const match1Emphasis: UrlEmphasis = {
     deemphasisMode: UrlDeemphasisMode.kAllButHost,
     textIsUrl: true,
     schemeRange: {start: 0, end: 5},
     hostRange: {start: 8, end: 20},
     schemeStrikeThrough: false,
     schemeDangerous: false,
   };

   const zpsMatches = [
     createSearchMatchForTesting({
       allowedToBeDefaultMatch: false,
       isSearchType: false,
       fillIntoEdit: 'https://redhat.com',
       destinationUrl: 'https://redhat.com',
       urlEmphasis: match0Emphasis,
     }),
     createSearchMatchForTesting({
       allowedToBeDefaultMatch: false,
       isSearchType: false,
       fillIntoEdit: 'https://chromium.org',
       destinationUrl: 'https://chromium.org',
       urlEmphasis: match1Emphasis,
     }),
   ];

   // 1. Elided steady-state URL (`fullUrlShown_ === false`): returns
   // `urlEmphasis_`.
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: 'example.com/page',
     permanentDisplayText: 'example.com/page',
     fullUrl: 'https://example.com/page',
     urlEmphasis: elidedEmphasis,
     unelidedUrlEmphasis: unelidedEmphasis,
     userInputInProgress: false,
     showFullUrl: false,
     isFocused: true,
     isTabSwitch: true,
     matchesUpdate: MatchesUpdate.kQueryZps,
   }));
   await microtasksFinished();

   testProxy.page.autocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: searchbox.activeQueryId,
     input: 'example.com/page',
     matches: zpsMatches,
   }));
   await microtasksFinished();
   assertEquals(-1, searchbox.matchIndex);
   assertDeepEquals(elidedEmphasis, searchbox.$.input.urlEmphasis);

   // Arrow down to match 0 and then match 1; emphasis updates to the
   // selected match's emphasis.
   await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
     bubbles: true,
     cancelable: true,
     key: 'ArrowDown',
   }));
   await microtasksFinished();
   assertDeepEquals(match0Emphasis, searchbox.$.input.urlEmphasis);

   await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
     bubbles: true,
     cancelable: true,
     key: 'ArrowDown',
   }));
   await microtasksFinished();
   assertDeepEquals(match1Emphasis, searchbox.$.input.urlEmphasis);

   // Escape Stage 1 (`kRevertTemporaryText` when default match is not
   // allowed to be default) restores elided steady-state URL emphasis.
   await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
     bubbles: true,
     cancelable: true,
     key: 'Escape',
   }));
   await microtasksFinished();
   assertDeepEquals(elidedEmphasis, searchbox.$.input.urlEmphasis);

   // 2. Unelided steady-state URL (`fullUrlShown_ === true`): returns
   // `unelidedUrlEmphasis_`.
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: 'example.com/page',
     permanentDisplayText: 'example.com/page',
     fullUrl: 'https://example.com/page',
     urlEmphasis: elidedEmphasis,
     unelidedUrlEmphasis: unelidedEmphasis,
     userInputInProgress: false,
     showFullUrl: true,
     isFocused: true,
     isTabSwitch: true,
     matchesUpdate: MatchesUpdate.kQueryZps,
   }));
   await microtasksFinished();

   testProxy.page.autocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: searchbox.activeQueryId,
     input: 'example.com/page',
     matches: zpsMatches,
   }));
   await microtasksFinished();
   assertEquals(-1, searchbox.matchIndex);
   assertDeepEquals(unelidedEmphasis, searchbox.$.input.urlEmphasis);

   // Select match 0 and match 1 (overwriting the unelided URL with
   // temporary match text), then revert via Escape Stage 1; since the
   // input text reverts to the elided `lastQueriedInput`, emphasis returns
   // to `elidedEmphasis`.
   await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
     bubbles: true,
     cancelable: true,
     key: 'ArrowDown',
   }));
   await microtasksFinished();
   assertDeepEquals(match0Emphasis, searchbox.$.input.urlEmphasis);

   await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
     bubbles: true,
     cancelable: true,
     key: 'ArrowDown',
   }));
   await microtasksFinished();
   assertDeepEquals(match1Emphasis, searchbox.$.input.urlEmphasis);

   await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
     bubbles: true,
     cancelable: true,
     key: 'Escape',
   }));
   await microtasksFinished();
   assertEquals(
       'example.com/page', searchbox.getInputElement().inputElement.value);
   assertDeepEquals(elidedEmphasis, searchbox.$.input.urlEmphasis);

   searchbox.setSelection(kDefaultSelection);
   await microtasksFinished();
   await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
     bubbles: true,
     cancelable: true,
     key: 'ArrowDown',
   }));
   await microtasksFinished();
   assertDeepEquals(match0Emphasis, searchbox.$.input.urlEmphasis);

   searchbox.setSelection(kDefaultSelection);
   await microtasksFinished();
   searchbox.updateInputForSelection(kDefaultSelection, 'Tab');
   assertEquals(
       'example.com/page', searchbox.getInputElement().inputElement.value);
   assertDeepEquals(elidedEmphasis, searchbox.$.input.urlEmphasis);

   // 3. Unelided steady-state URL when `unelidedUrlEmphasis_` is null:
   // falls back to `urlEmphasis_`.
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: 'example.com/page',
     permanentDisplayText: 'example.com/page',
     fullUrl: 'https://example.com/page',
     urlEmphasis: elidedEmphasis,
     unelidedUrlEmphasis: null,
     userInputInProgress: false,
     showFullUrl: true,
     isFocused: true,
     isTabSwitch: true,
     matchesUpdate: MatchesUpdate.kQueryZps,
   }));
   await microtasksFinished();

   testProxy.page.autocompleteResultChanged(createAutocompleteResultForTesting({
     queryId: searchbox.activeQueryId,
     input: 'example.com/page',
     matches: zpsMatches,
   }));
   await microtasksFinished();
   assertDeepEquals(elidedEmphasis, searchbox.$.input.urlEmphasis);
 });

 test('UpdatesLayoutConstantsFromLoadTimeData', async () => {
   const originalConstants = {
     alignmentInsetTop: loadTimeData.getInteger('alignmentInsetTop'),
     alignmentInsetHorizontal:
         loadTimeData.getInteger('alignmentInsetHorizontal'),
     alignmentInsetBottom: loadTimeData.getInteger('alignmentInsetBottom'),
   };

   // Verify searchbox in `setup()` initialized its inline styles with the
   // C++ alignment insets provided by `OmniboxPopupUI`.
   assertEquals(
       `${originalConstants.alignmentInsetTop}px`,
       searchbox.style.getPropertyValue('--alignment-inset-top'));

   // Override `loadTimeData` with custom alignment insets and verify a newly
   // connected searchbox applies them as inline style custom properties.
   loadTimeData.overrideValues({
     alignmentInsetTop: 10,
     alignmentInsetHorizontal: 12,
     alignmentInsetBottom: 8,
   });

   const testSearchbox = document.createElement('omnibox-popup-searchbox');
   document.body.appendChild(testSearchbox);
   await microtasksFinished();

   assertEquals(
       '10px', testSearchbox.style.getPropertyValue('--alignment-inset-top'));
   assertEquals(
       '12px',
       testSearchbox.style.getPropertyValue('--alignment-inset-horizontal'));
   assertEquals(
       '8px', testSearchbox.style.getPropertyValue('--alignment-inset-bottom'));

   testSearchbox.remove();
   // Restore original values so `loadTimeData` does not leak into other tests.
   loadTimeData.overrideValues(originalConstants);
 });

  test('ComposeButtonDoesNotShiftWhenDropdownOpens', async () => {
    // Match `omnibox_popup_document_style.css` (`body { margin: 0; }`) so
    // `:host(:not([dropdown-is-visible]))`'s top margin does not collapse with
    // the default 8px user-agent body margin.
    const originalMargin = document.body.style.margin;
    document.body.style.margin = '0';
    searchbox.dropdownIsVisible = false;
    testProxy.page.setAimButtonVisible(true);
    await microtasksFinished();

    const composeButton = searchbox.$.composeButton;
    assertTrue(isVisible(composeButton));
    const glowContainer =
        composeButton.shadowRoot.querySelector<HTMLElement>('.glow-container');
    assertTrue(!!glowContainer);

    const closedGlowRect = glowContainer.getBoundingClientRect();

    // Open the dropdown by populating autocomplete matches.
    searchbox.activeQueryId = 0;
    searchbox.onAutocompleteResultChanged(createAutocompleteResultForTesting({
      queryId: 0,
      input: 'test',
      matches: [createSearchMatchForTesting({allowedToBeDefaultMatch: true})],
    }));
    await microtasksFinished();
    assertTrue(searchbox.dropdownIsVisible);

    const openGlowRect = glowContainer.getBoundingClientRect();
    assertEquals(closedGlowRect.top, openGlowRect.top);
    assertEquals(closedGlowRect.bottom, openGlowRect.bottom);
    assertEquals(closedGlowRect.left, openGlowRect.left);
    assertEquals(closedGlowRect.right, openGlowRect.right);

    document.body.style.margin = originalMargin;
  });
});

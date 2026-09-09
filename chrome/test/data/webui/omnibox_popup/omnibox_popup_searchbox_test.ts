// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isMac} from '//resources/js/platform.js';
import {OmniboxEscapeAction, omniboxPopupBrowserProxyFactory, OmniboxPopupPageHandlerRemote, sanitizeTextForPaste, SearchboxBrowserProxy, stripJavascriptSchemas} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import type {OmniboxInputState, OmniboxPopupContextualEntrypointButtonElement, OmniboxPopupPageRemote, OmniboxPopupSearchboxElement} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {createAutocompleteResultForTesting, createMatchKeywordModelForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {KeywordType, RenderType, SelectionLineState, SideType} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {TabInfo} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

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
    queryZps: false,
    keywordModel: null,
    ...overrides,
  };
}

suite('OmniboxPopupSearchboxTest', function() {
  let searchbox: OmniboxPopupSearchboxElement;
  let testProxy: TestSearchboxBrowserProxy;
  let handler: TestMock<OmniboxPopupPageHandlerRemote>&
      OmniboxPopupPageHandlerRemote;
  let callbackRouter: OmniboxPopupPageRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      hideClassicContextButton: false,
      composeboxShowContextMenuDescription: false,
      omniboxShowContextButtonSuggestionLabel: false,
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
    searchbox.$.input.dispatchEvent(
        new CustomEvent('searchbox-input-text-updated', {
          bubbles: true,
          composed: true,
          detail: {value: 'tab 1 draft edited', isComposing: false},
        }));
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
    }));
    await microtasksFinished();

    // Tab 2 must have its edit history reset, preventing Tab 1 edits from
    // leaking.
    [canUndo, canRedo] = await handler.whenCalled('setEditHistoryState');
    assertFalse(canUndo);
    assertFalse(canRedo);
  });

  test('EnterKeySubmitsVerbatimMatchWhenNoMatchSelected', async () => {
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

    const [line, url, areMatchesShowing, mouseButton, modifiers, viaKeyboard] =
        await testProxy.handler.whenCalled('openAutocompleteMatch');
    assertEquals(-1, line);
    assertEquals('', url);
    assertFalse(areMatchesShowing);
    assertEquals(0, mouseButton);
    assertFalse(modifiers.altKey);
    assertFalse(modifiers.ctrlKey);
    assertFalse(modifiers.metaKey);
    assertFalse(modifiers.shiftKey);
    assertTrue(viaKeyboard);
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

  test('HandlesRevert', async () => {
    // Test revert is called with default sequence number (0).
    searchbox.revert();
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('revert'));
    assertEquals(0, handler.getArgs('revert')[0]);

    // Test revert is called with active sequence number (42) after receiving
    // state.
    handler.reset();
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      sequenceNumber: 42,
      text: 'hello',
      selection: {start: 5, end: 5},
      userInputInProgress: true,
      isFocused: true,
    }));
    await microtasksFinished();

    searchbox.revert();
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('revert'));
    assertEquals(42, handler.getArgs('revert')[0]);
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

  test('ExecutesDeferredFocusOnVisibilityChange', async () => {
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
  // `document.visibilityState` is hidden, focus and select-all are deferred
  // until `visibilitychange` occurs (`DeferredFocusAction.FOCUS_AND_SELECT`).
  test('ExecutesDeferredFocusAndSelectOnVisibilityChange', async () => {
    Object.defineProperty(
        document, 'visibilityState', {value: 'hidden', configurable: true});

    // Trigger dedicated `setFocus` IPC while document is hidden.
    callbackRouter.setFocus(true, false);
    await microtasksFinished();

    const input = searchbox.$.input.inputElement;
    assertFalse(searchbox.shadowRoot.activeElement === searchbox.$.input);

    // Make visible and dispatch `visibilitychange` event.
    Object.defineProperty(
        document, 'visibilityState', {value: 'visible', configurable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await microtasksFinished();

    // Verify both focus AND select occurred
    // (`DeferredFocusAction.FOCUS_AND_SELECT`).
    assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
    assertEquals(0, input.selectionStart);
    assertEquals(input.value.length, input.selectionEnd);
  });

  test('SetFocus_RequeriesZpsWhenSteadyStateAndDropdownClosed', async () => {
    const testUrl = 'https://example.com';
    callbackRouter.setInputState(createDefaultOmniboxInputState({
      text: testUrl,
      userInputInProgress: false,
      isFocused: true,
      queryZps: false,
    }));
    await microtasksFinished();

    searchbox.clearAutocompleteMatches();
    assertFalse(searchbox.dropdownIsVisible);
    testProxy.handler.resetResolver('queryAutocomplete');

    callbackRouter.setFocus(true, /*queryZps=*/ true);
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
      userInputInProgress: true,
      isFocused: true,
      queryZps: false,
    }));
    await microtasksFinished();

    searchbox.dropdownIsVisible = true;
    testProxy.handler.resetResolver('queryAutocomplete');

    callbackRouter.setFocus(true, /*queryZps=*/ true);
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
      userInputInProgress: false,
      isFocused: true,
      queryZps: false,
    }));
    await microtasksFinished();

    searchbox.dropdownIsVisible = true;
    testProxy.handler.resetResolver('queryAutocomplete');

    callbackRouter.setFocus(true, /*queryZps=*/ true);
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
      userInputInProgress: false,
      isFocused: true,
      queryZps: false,
    }));
    await microtasksFinished();

    searchbox.clearAutocompleteMatches();
    assertFalse(searchbox.dropdownIsVisible);
    testProxy.handler.resetResolver('queryAutocomplete');

    callbackRouter.setFocus(true, /*queryZps=*/ false);
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
    assertEquals(
        inputElement.scrollWidth - inputElement.clientWidth,
        inputElement.scrollLeft);
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
  });

  test('WordDeletionUndoRedo', async () => {
    searchbox.$.input.focus();
    const inputEl = searchbox.$.input.inputElement;

    // Baseline text: "hello world" with caret at index 11.
    inputEl.value = 'hello world';
    inputEl.setSelectionRange(11, 11);
    searchbox.$.input.dispatchEvent(
        new CustomEvent('searchbox-input-text-updated', {
          detail: {value: 'hello world', isComposing: false},
          bubbles: true,
          composed: true,
        }));
    await microtasksFinished();

    // User performs word deletion (Alt/Ctrl + Backspace): text becomes "hello
    // ", cursor moves to 6.
    inputEl.value = 'hello ';
    inputEl.selectionStart = 6;
    inputEl.selectionEnd = 6;
    searchbox.$.input.dispatchEvent(
        new CustomEvent('searchbox-input-text-updated', {
          detail: {value: 'hello ', isComposing: false},
          bubbles: true,
          composed: true,
        }));
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
   callbackRouter.setFocus(false, false);
   await microtasksFinished();
   assertFalse(searchbox.dropdownIsVisible);
 });

 test('EscapeStagedUnwinding', async () => {
   // Stage 1 (`kRevertTemporaryText`):
   searchbox.getInputElement().inputElement.value = 'a';
   searchbox.lastQueriedInput = 'a';
   searchbox.activeQueryId = 0;
   testProxy.page.autocompleteResultChanged(createAutocompleteResultForTesting({
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

   searchbox.selectedMatchIndex = 1;
   searchbox.getInputElement().inputElement.value = 'suggestion-1';
   await microtasksFinished();

   await searchbox.handleKeyNavigation(new KeyboardEvent('keydown', {
     key: 'Escape',
     cancelable: true,
   }));
   await microtasksFinished();

   assertEquals('a', searchbox.getInputElement().inputElement.value);
   assertEquals(0, searchbox.selectedMatchIndex);
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
       OmniboxEscapeAction.kClosePopup, handler.getArgs('logEscapeAction')[0]);

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

   assertEquals('example.com', searchbox.getInputElement().inputElement.value);
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

 test('CmdCtrlL_SelectsTextWhenUserInputInProgress', async () => {
   const draftQuery = 'chrome query';
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: draftQuery,
     userInputInProgress: true,
     isFocused: true,
     queryZps: false,
   }));
   await microtasksFinished();

   const inputEl = searchbox.getInputElement().inputElement;
   inputEl.setSelectionRange(2, 2);
   testProxy.handler.resetResolver('queryAutocomplete');

   inputEl.dispatchEvent(new KeyboardEvent('keydown', {
     key: 'l',
     ctrlKey: !isMac,
     metaKey: isMac,
     bubbles: true,
     composed: true,
   }));
   await microtasksFinished();

   assertEquals(0, inputEl.selectionStart);
   assertEquals(draftQuery.length, inputEl.selectionEnd);
   assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
 });

 test(
     'CmdCtrlL_SelectsTextAndQueriesZpsWhenUserInputNotInProgress',
     async () => {
       const testUrl = 'https://example.com';
       callbackRouter.setInputState(createDefaultOmniboxInputState({
         text: testUrl,
         userInputInProgress: false,
         isFocused: true,
         queryZps: false,
       }));
       await microtasksFinished();

       searchbox.clearAutocompleteMatches();
       assertFalse(searchbox.dropdownIsVisible);
       testProxy.handler.resetResolver('queryAutocomplete');

       const inputEl = searchbox.getInputElement().inputElement;
       inputEl.setSelectionRange(3, 3);

       inputEl.dispatchEvent(new KeyboardEvent('keydown', {
         key: 'l',
         ctrlKey: !isMac,
         metaKey: isMac,
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

 test('CmdCtrlL_DoesNotRequeryZpsWhenDropdownAlreadyOpen', async () => {
   const testUrl = 'https://example.com';
   callbackRouter.setInputState(createDefaultOmniboxInputState({
     text: testUrl,
     userInputInProgress: false,
     isFocused: true,
     queryZps: false,
   }));
   await microtasksFinished();

   searchbox.dropdownIsVisible = true;
   testProxy.handler.resetResolver('queryAutocomplete');

   const inputEl = searchbox.getInputElement().inputElement;
   inputEl.setSelectionRange(3, 3);

   inputEl.dispatchEvent(new KeyboardEvent('keydown', {
     key: 'l',
     ctrlKey: !isMac,
     metaKey: isMac,
     bubbles: true,
     composed: true,
   }));
   await microtasksFinished();

   assertEquals(0, inputEl.selectionStart);
   assertEquals(testUrl.length, inputEl.selectionEnd);
   assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
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
   searchbox.$.input.dispatchEvent(
       new CustomEvent('searchbox-input-text-updated', {
         detail: {value: 'hello!', isComposing: false},
         bubbles: true,
         composed: true,
       }));
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
   searchbox.$.input.dispatchEvent(
       new CustomEvent('searchbox-input-text-updated', {
         detail: {value: 'world', isComposing: false},
         bubbles: true,
         composed: true,
       }));
   await microtasksFinished();

   // Step 2: User selects all text 'world' (0..5).
   inputEl.setSelectionRange(0, 5);
   document.dispatchEvent(new Event('selectionchange'));
   await microtasksFinished();

   // Step 3: User replaces selected 'world' with 'universe'.
   inputEl.value = 'universe';
   inputEl.setSelectionRange(8, 8);
   searchbox.$.input.dispatchEvent(
       new CustomEvent('searchbox-input-text-updated', {
         detail: {value: 'universe', isComposing: false},
         bubbles: true,
         composed: true,
       }));
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
         omniboxShowContextButtonSuggestionLabel: false,
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
         omniboxShowContextButtonSuggestionLabel: false,
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
         omniboxShowContextButtonSuggestionLabel: false,
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
   }));
   await microtasksFinished();

   // Ensure matches and dropdown are cleared for the new input state.
   assertFalse(searchbox.dropdownIsVisible);
   assertFalse(!!searchbox.result);
 });

 suite('InputIconState', () => {
   test('InputMatchUpdatesWithSelectedMatch', async () => {
     const navMatch = createSearchMatchForTesting({
       contents: 'example.com',
       destinationUrl: 'https://example.com/',
       isSearchType: false,
       type: 'history-url',
     });
     const searchMatch = createSearchMatchForTesting({
       contents: 'example query',
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
     assertEquals('example query', searchbox.$.input.selectedMatch?.contents);

     // When navigating to match 0, input's selectedMatch should be match 0.
     searchbox.selectedMatchIndex = 0;
     await microtasksFinished();
     assertEquals(
         searchbox.result!.matches[0], searchbox.$.input.selectedMatch);
     assertEquals('example.com', searchbox.$.input.selectedMatch?.contents);

     // When navigating back to unselected, input's selectedMatch should return
     // to null.
     searchbox.selectedMatchIndex = -1;
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
   const keyword = 'youtube.com';
   const match = createSearchMatchForTesting({
     allowedToBeDefaultMatch: true,
     contents: 'youtube',
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
   const match = createSearchMatchForTesting({
     allowedToBeDefaultMatch: true,
     contents: 'youtube.com',
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
   callbackRouter.setFocus(true, /*queryZps=*/ false);
   testProxy.page.setAimButtonVisible(true);
   await microtasksFinished();

   const composeButton = searchbox.$.composeButton;
   assertTrue(!!composeButton);
   assertTrue(isVisible(composeButton));

   // When focus is lost, AIM button should be hidden.
   callbackRouter.setFocus(false, /*queryZps=*/ false);
   await microtasksFinished();

   assertFalse(isVisible(composeButton));

   // Refocus, then type text into the Omnibox.
   callbackRouter.setFocus(true, /*queryZps=*/ true);
   searchbox.getInputElement().setInputText('temporary text');
   testProxy.page.setAimButtonVisible(true);
   await microtasksFinished();

   assertTrue(isVisible(composeButton));

   // If focus is lost with temporary text in the Omnibox, then AIM button
   // should be hidden.
   callbackRouter.setFocus(false, /*queryZps=*/ false);
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
});

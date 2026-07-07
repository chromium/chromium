// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {omniboxPopupBrowserProxyFactory, OmniboxPopupPageHandlerRemote, SearchboxBrowserProxy} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import type {OmniboxPopupPageRemote, OmniboxPopupSearchboxElement} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

suite('OmniboxPopupSearchboxTest', function() {
  let searchbox: OmniboxPopupSearchboxElement;
  let testProxy: TestSearchboxBrowserProxy;
  let handler: TestMock<OmniboxPopupPageHandlerRemote>&
      OmniboxPopupPageHandlerRemote;
  let callbackRouter: OmniboxPopupPageRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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
    callbackRouter.setInputState({
      sequenceNumber: 1,
      text: testText,
      selection: {start: 0, end: 0},
      userInputInProgress: false,
      fullUrl: '',
      isFocused: false,
      permanentDisplayText: '',
      showFullUrl: false,
    });
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

  test('HandlesSelectionChange', async () => {
    // Focus the input so it's the active element.
    const input = searchbox.$.input.inputElement;
    input.focus();
    await microtasksFinished();
    // Set some text in the omnibox popup via Mojo.
    callbackRouter.setInputState({
      sequenceNumber: 123,
      text: 'test text',
      selection: {start: 0, end: 0},
      userInputInProgress: true,
      fullUrl: '',
      isFocused: true,
      permanentDisplayText: '',
      showFullUrl: false,
    });
    await microtasksFinished();

    // Send `focusin` event to clear `pendingFocusSelection_`.
    searchbox.$.input.dispatchEvent(new Event('focusin', {bubbles: true}));
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
    callbackRouter.setInputState({
      sequenceNumber: 1,
      text: 'test text',
      selection: {start: 1, end: 4},
      userInputInProgress: false,
      fullUrl: '',
      isFocused: false,
      permanentDisplayText: '',
      showFullUrl: false,
    });
    await microtasksFinished();

    // Ensure selection was applied immediately regardless of focus.
    assertEquals(1, input.selectionStart);
    assertEquals(4, input.selectionEnd);
  });

  test('RejectsFocusWhenUserInputInProgress', async () => {
    callbackRouter.setInputState({
      sequenceNumber: 1,
      text: 'edited text',
      selection: {start: 0, end: 0},
      userInputInProgress: true,
      fullUrl: '',
      isFocused: true,
      permanentDisplayText: '',
      showFullUrl: false,
    });
    await microtasksFinished();

    searchbox.onInputFocusChanged(new CustomEvent(
        'input-focus-changed', {detail: {value: 'edited text'}}));
    assertEquals(0, testProxy.handler.getCallCount('queryAutocomplete'));
    assertFalse(searchbox.dropdownIsVisible);

    callbackRouter.setInputState({
      sequenceNumber: 2,
      text: 'permanent text',
      selection: {start: 0, end: 0},
      userInputInProgress: false,
      fullUrl: '',
      isFocused: true,
      permanentDisplayText: '',
      showFullUrl: false,
    });
    await microtasksFinished();

    searchbox.onInputFocusChanged(new CustomEvent(
        'input-focus-changed', {detail: {value: 'permanent text'}}));
    assertEquals(1, testProxy.handler.getCallCount('queryAutocomplete'));
  });

  test('IgnoresStaleAutocompleteResults', async () => {
    // Simulate user typing a custom query.
    callbackRouter.setInputState({
      sequenceNumber: 1,
      text: 'custom draft',
      selection: {start: 12, end: 12},
      userInputInProgress: true,
      fullUrl: '',
      isFocused: true,
      permanentDisplayText: '',
      showFullUrl: false,
    });
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

    callbackRouter.setInputState({
      sequenceNumber: 1,
      text: 'CJK text',
      selection: {start: 0, end: 0},
      userInputInProgress: true,
      fullUrl: '',
      isFocused: true,
      permanentDisplayText: '',
      showFullUrl: false,
    });
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


    callbackRouter.setInputState({
      sequenceNumber: 1,
      text: 'test.com',
      selection: {start: 0, end: 4},
      userInputInProgress: true,
      fullUrl: full_url,
      isFocused: true,
      permanentDisplayText: '',
      showFullUrl: false,
    });
    await microtasksFinished();
    handler.reset();

    // Verify the full URL is displayed.
    assertEquals(full_url, input.value);
  });

  test('HandlesSetInputStateFocus', async () => {
    // Set isFocused = true.
    callbackRouter.setInputState({
      sequenceNumber: 1,
      text: 'test text',
      selection: {start: 0, end: 0},
      userInputInProgress: false,
      fullUrl: '',
      isFocused: true,
      permanentDisplayText: '',
      showFullUrl: false,
    });
    await microtasksFinished();

    // Verify input element is focused.
    assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);

    // Set isFocused = false.
    callbackRouter.setInputState({
      sequenceNumber: 2,
      text: 'test text',
      selection: {start: 0, end: 0},
      userInputInProgress: false,
      fullUrl: '',
      isFocused: false,
      permanentDisplayText: '',
      showFullUrl: false,
    });
    await microtasksFinished();

    // Verify input element is blurred.
    assertFalse(searchbox.$.input === searchbox.shadowRoot.activeElement);
  });

  // TODO(crbug.com/529516876): Fix and re-enable
  test.skip('HandlesManualBlur', async () => {
    // Test default sequence number (0) when no state is set.
    const input = searchbox.$.input.inputElement;
    input.focus();
    await microtasksFinished();

    assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
    input.blur();
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('onManualBlur'));
    assertEquals(0, handler.getArgs('onManualBlur')[0]);
    assertFalse(searchbox.$.input === searchbox.shadowRoot.activeElement);

    // Test active sequence number (42) after receiving state.
    handler.reset();
    callbackRouter.setInputState({
      sequenceNumber: 42,
      text: 'hello',
      selection: {start: 5, end: 5},
      userInputInProgress: true,
      fullUrl: '',
      isFocused: true,
      permanentDisplayText: '',
      showFullUrl: false,
    });
    await microtasksFinished();

    input.blur();
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('onManualBlur'));
    assertEquals(42, handler.getArgs('onManualBlur')[0]);
  });

  test('HandlesRevert', async () => {
    // Test revert is called with default sequence number (0).
    searchbox.clearAutocompleteMatches();
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('revert'));
    assertEquals(0, handler.getArgs('revert')[0]);

    // Test revert is called with active sequence number (42) after receiving
    // state.
    handler.reset();
    callbackRouter.setInputState({
      sequenceNumber: 42,
      text: 'hello',
      selection: {start: 5, end: 5},
      userInputInProgress: true,
      fullUrl: '',
      isFocused: true,
      permanentDisplayText: '',
      showFullUrl: false,
    });
    await microtasksFinished();

    searchbox.clearAutocompleteMatches();
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('revert'));
    assertEquals(42, handler.getArgs('revert')[0]);
  });

  test('IgnoresManualBlurWhenWindowInactive', async () => {
    // Override `document.visibilityState` to 'hidden'.
    Object.defineProperty(document, 'visibilityState', {
      value: 'hidden',
      configurable: true,
    });

    const input = searchbox.$.input.inputElement;
    input.focus();
    await microtasksFinished();

    assertEquals(searchbox.$.input, searchbox.shadowRoot.activeElement);
    input.blur();
    await microtasksFinished();

    // Verify `onManualBlur` is NOT called because `document.visibilityState` is
    // hidden.
    assertEquals(0, handler.getCallCount('onManualBlur'));

    // Restore `document.visibilityState` to 'visible'.
    Object.defineProperty(document, 'visibilityState', {
      value: 'visible',
      configurable: true,
    });
  });
});

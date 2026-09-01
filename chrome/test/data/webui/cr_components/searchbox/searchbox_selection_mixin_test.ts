// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {createActionForTesting, createAutocompleteMatch, createAutocompleteResultForTesting, createMatchKeywordModelForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {kDefaultSelection} from 'chrome://resources/cr_components/searchbox/searchbox_match.js';
import {SearchboxSelectionMixin, selectionIsNativelySupported, selectionsEqual} from 'chrome://resources/cr_components/searchbox/searchbox_selection_mixin.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteResult} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {KeywordType, PageHandlerRemote, SelectionDirection, SelectionLineState, SelectionStep} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

interface MockInputElement {
  inputElement: {value: string};
  focus: () => void;
  setInput:
      (options:
           {text: string, inline: string, moveCursorToEnd: boolean}) => void;
}

const TestElementBase = SearchboxSelectionMixin(CrLitElement);

class TestSearchboxSelectionMixinElement extends TestElementBase {
  static get is() {
    return 'test-searchbox-selection-mixin';
  }
  isAimVisible: boolean = false;
  showEntrypoint: boolean = false;
  dropdownIsVisible: boolean = true;
  result: AutocompleteResult|null = null;
  mockPageHandler: TestMock<PageHandlerRemote> =
      TestMock.fromClass(PageHandlerRemote);
  mockInputElement: MockInputElement = {
    inputElement: {value: ''},
    focus: () => {},
    setInput: () => {},
  };

  override get isAimButtonVisible() {
    return this.isAimVisible;
  }

  override get showContextEntrypoint() {
    return this.showEntrypoint;
  }
  pageHandler() {
    return this.mockPageHandler;
  }
  getInputElement() {
    return this.mockInputElement;
  }
  get selectedMatch() {
    return this.result?.matches[this.selection.line] ?? null;
  }
}
customElements.define(TestSearchboxSelectionMixinElement.is, TestSearchboxSelectionMixinElement);

suite('CrComponentsSearchboxSelectionMixinTest', () => {
  let element: TestSearchboxSelectionMixinElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = new TestSearchboxSelectionMixinElement();
    document.body.appendChild(element);
  });

  test('selectionsEqual', () => {
    assertTrue(selectionsEqual(
        {line: 1, state: SelectionLineState.kNormal, actionIndex: 0},
        {line: 1, state: SelectionLineState.kNormal, actionIndex: 0}));

    assertFalse(selectionsEqual(
        {line: 1, state: SelectionLineState.kNormal, actionIndex: 0},
        {line: 2, state: SelectionLineState.kNormal, actionIndex: 0}));

    assertFalse(selectionsEqual(
        {line: 1, state: SelectionLineState.kNormal, actionIndex: 0},
        {line: 1, state: SelectionLineState.kKeywordMode, actionIndex: 0}));
  });

  test('getAvailableSelections', () => {
    const match1 = createAutocompleteMatch();
    const result = createAutocompleteResultForTesting({
      matches: [match1],
    });

    element.isAimVisible = true;
    element.showEntrypoint = false;
    let selections = element.getAvailableSelections(result);
    assertEquals(3, selections.length);
    assertDeepEquals(selections[0], {
      line: -1,
      state: SelectionLineState.kNormal,
      actionIndex: 0,
    });
    assertDeepEquals(selections[1], {
      line: -1,
      state: SelectionLineState.kFocusedButtonAim,
      actionIndex: 0,
    });
    assertDeepEquals(selections[2], {
      line: 0,
      state: SelectionLineState.kNormal,
      actionIndex: 0,
    });

    element.isAimVisible = false;
    element.showEntrypoint = true;
    selections = element.getAvailableSelections(result);
    assertEquals(2, selections.length);
    assertDeepEquals(
        selections[0],
        {line: 0, state: SelectionLineState.kNormal, actionIndex: 0});
    assertDeepEquals(selections[1], {
      line: -1,
      state: SelectionLineState.kFocusedButtonContextEntrypoint,
      actionIndex: 0,
    });
  });

  test('stepCyclesSelection', () => {
    const match1 = createAutocompleteMatch();
    const match2 = createAutocompleteMatch();
    const match3 = createAutocompleteMatch();
    const result = createAutocompleteResultForTesting({
      matches: [match1, match2, match3],
    });

    const available = element.getAvailableSelections(result);
    assertEquals(3, available.length);

    // Forward direction with kStateOrLine: only stepping from the last item
    // cycles.
    assertFalse(element.stepCyclesSelection(
        result, available[0]!, SelectionDirection.kForward,
        SelectionStep.kStateOrLine));
    assertFalse(element.stepCyclesSelection(
        result, available[1]!, SelectionDirection.kForward,
        SelectionStep.kStateOrLine));
    assertTrue(element.stepCyclesSelection(
        result, available[2]!, SelectionDirection.kForward,
        SelectionStep.kStateOrLine));

    // Backward direction with kStateOrLine: only stepping from the first item
    // cycles.
    assertTrue(element.stepCyclesSelection(
        result, available[0]!, SelectionDirection.kBackward,
        SelectionStep.kStateOrLine));
    assertFalse(element.stepCyclesSelection(
        result, available[1]!, SelectionDirection.kBackward,
        SelectionStep.kStateOrLine));
    assertFalse(element.stepCyclesSelection(
        result, available[2]!, SelectionDirection.kBackward,
        SelectionStep.kStateOrLine));

    // Edge case: match with trailing sub-button (e.g. remove suggestion).
    const matchWithDeletion = createAutocompleteMatch({supportsDeletion: true});
    const resultWithDeletion = createAutocompleteResultForTesting({
      matches: [match1, matchWithDeletion],
    });
    const availableWithDeletion =
        element.getAvailableSelections(resultWithDeletion);
    assertEquals(3, availableWithDeletion.length);
    // availableWithDeletion[0] = line 0 (normal)
    // availableWithDeletion[1] = line 1 (normal)
    // availableWithDeletion[2] = line 1 (remove suggestion button)

    // Stepping from line 1 with kStateOrLine goes to the remove button (no
    // cycle).
    assertFalse(element.stepCyclesSelection(
        resultWithDeletion, availableWithDeletion[1]!,
        SelectionDirection.kForward, SelectionStep.kStateOrLine));

    // Stepping from line 1 with kWholeLine skips the remove button and cycles
    // to line 0.
    assertTrue(element.stepCyclesSelection(
        resultWithDeletion, availableWithDeletion[1]!,
        SelectionDirection.kForward, SelectionStep.kWholeLine));

    // Empty or null results always cycle.
    assertTrue(element.stepCyclesSelection(
        null, available[0]!, SelectionDirection.kForward,
        SelectionStep.kStateOrLine));

    const emptyResult = createAutocompleteResultForTesting({matches: []});
    assertTrue(element.stepCyclesSelection(
        emptyResult, available[0]!, SelectionDirection.kForward,
        SelectionStep.kStateOrLine));

    const singleMatchResult = createAutocompleteResultForTesting({
      matches: [match1],
    });
    const singleAvailable = element.getAvailableSelections(singleMatchResult);
    assertEquals(1, singleAvailable.length);

    // Stepping forward from unselected input enters the single item (no cycle).
    assertFalse(element.stepCyclesSelection(
        singleMatchResult, kDefaultSelection, SelectionDirection.kForward,
        SelectionStep.kStateOrLine));

    // Stepping backward from unselected input cycles/exits immediately.
    assertTrue(element.stepCyclesSelection(
        singleMatchResult, kDefaultSelection, SelectionDirection.kBackward,
        SelectionStep.kStateOrLine));

    // Once already on the single item, any step in either direction cycles.
    assertTrue(element.stepCyclesSelection(
        singleMatchResult, singleAvailable[0]!, SelectionDirection.kForward,
        SelectionStep.kStateOrLine));
    assertTrue(element.stepCyclesSelection(
        singleMatchResult, singleAvailable[0]!, SelectionDirection.kBackward,
        SelectionStep.kStateOrLine));
  });

  test('getNextSelection Forward Line', () => {
    const match1 = createAutocompleteMatch();
    const match2 = createAutocompleteMatch();
    const match3 = createAutocompleteMatch();
    const result = createAutocompleteResultForTesting({
      matches: [match1, match2, match3],
    });

    const available = element.getAvailableSelections(result);
    assertEquals(3, available.length);

    const next = element.getNextSelection(
        result, available[0]!, SelectionDirection.kForward,
        SelectionStep.kStateOrLine);
    assertDeepEquals(available[1]!, next);

    // getNextSelection always wraps cyclically.
    const nextEndCycle = element.getNextSelection(
        result, available[2]!, SelectionDirection.kForward,
        SelectionStep.kStateOrLine);
    assertDeepEquals(available[0]!, nextEndCycle);
  });

  test('getNextSelection Backward Line', () => {
    const match1 = createAutocompleteMatch();
    const match2 = createAutocompleteMatch();
    const match3 = createAutocompleteMatch();
    const result = createAutocompleteResultForTesting({
      matches: [match1, match2, match3],
    });

    const available = element.getAvailableSelections(result);

    const prev = element.getNextSelection(
        result, available[1]!, SelectionDirection.kBackward,
        SelectionStep.kStateOrLine);
    assertDeepEquals(available[0]!, prev);

    // getNextSelection always wraps cyclically.
    const prevStartCycle = element.getNextSelection(
        result, available[0]!, SelectionDirection.kBackward,
        SelectionStep.kStateOrLine);
    assertDeepEquals(available[2]!, prevStartCycle);
  });

  test('getNextSelection AllLines (PageUp/PageDown)', () => {
    const match1 = createAutocompleteMatch();
    const match2 = createAutocompleteMatch();
    match2.supportsDeletion = true;  // adds a focused button
    const match3 = createAutocompleteMatch();
    const result = createAutocompleteResultForTesting({
      matches: [match1, match2, match3],
    });

    const available = element.getAvailableSelections(result);
    assertEquals(4, available.length);
    // index 2 is the delete button for match 2.

    // PageDown (Forward, AllLines) should go to the very last normal match.
    const pageDown = element.getNextSelection(
        result, available[0]!, SelectionDirection.kForward,
        SelectionStep.kAllLines);
    assertDeepEquals(available[3]!, pageDown);  // index 3 is normal match 3

    // PageUp (Backward, AllLines) should go to the very first normal match.
    const pageUp = element.getNextSelection(
        result, available[3]!, SelectionDirection.kBackward,
        SelectionStep.kAllLines);
    assertDeepEquals(available[0]!, pageUp);

    // Backward AllLines from unselected state (line -1) stays at line -1.
    const defaultSelection = {
      line: -1,
      state: SelectionLineState.kNormal,
      actionIndex: 0,
    };
    const backwardFromDefault = element.getNextSelection(
        result, defaultSelection, SelectionDirection.kBackward,
        SelectionStep.kAllLines);
    assertDeepEquals(defaultSelection, backwardFromDefault);
  });

  test('getNextSelection All SelectionLineState types', () => {
    const match = createAutocompleteMatch();
    match.keywordModel =
        createMatchKeywordModelForTesting({chipHint: 'Search keyword'});
    match.actions = [createActionForTesting(), createActionForTesting()];
    match.supportsDeletion = true;

    const result = createAutocompleteResultForTesting({
      matches: [match],
    });

    const available = element.getAvailableSelections(result);
    assertEquals(5, available.length);
    assertDeepEquals(
        {line: 0, state: SelectionLineState.kNormal, actionIndex: 0},
        available[0]);
    assertDeepEquals(
        {line: 0, state: SelectionLineState.kKeywordMode, actionIndex: 0},
        available[1]);
    assertDeepEquals(
        {
          line: 0,
          state: SelectionLineState.kFocusedButtonAction,
          actionIndex: 0,
        },
        available[2]);
    assertDeepEquals(
        {
          line: 0,
          state: SelectionLineState.kFocusedButtonAction,
          actionIndex: 1,
        },
        available[3]);
    assertDeepEquals(
        {
          line: 0,
          state: SelectionLineState.kFocusedButtonRemoveSuggestion,
          actionIndex: 0,
        },
        available[4]);

    // Forward
    assertDeepEquals(
        available[1]!,
        element.getNextSelection(
            result,
            available[0]!,
            SelectionDirection.kForward,
            SelectionStep.kStateOrLine,
            ));
    assertDeepEquals(
        available[2]!,
        element.getNextSelection(
            result, available[1]!, SelectionDirection.kForward,
            SelectionStep.kStateOrLine));
    assertDeepEquals(
        available[3]!,
        element.getNextSelection(
            result, available[2]!, SelectionDirection.kForward,
            SelectionStep.kStateOrLine));
    assertDeepEquals(
        available[4]!,
        element.getNextSelection(
            result, available[3]!, SelectionDirection.kForward,
            SelectionStep.kStateOrLine));

    // Backward
    assertDeepEquals(
        available[3]!,
        element.getNextSelection(
            result, available[4]!, SelectionDirection.kBackward,
            SelectionStep.kStateOrLine));
    assertDeepEquals(
        available[2]!,
        element.getNextSelection(
            result, available[3]!, SelectionDirection.kBackward,
            SelectionStep.kStateOrLine));
    assertDeepEquals(
        available[1]!,
        element.getNextSelection(
            result, available[2]!, SelectionDirection.kBackward,
            SelectionStep.kStateOrLine));
    assertDeepEquals(
        available[0]!,
        element.getNextSelection(
            result, available[1]!, SelectionDirection.kBackward,
            SelectionStep.kStateOrLine));
  });

  test('getNextSelection WholeLine', () => {
    const match1 = createAutocompleteMatch();
    match1.supportsDeletion = true;
    const match2 = createAutocompleteMatch();
    const result = createAutocompleteResultForTesting({
      matches: [match1, match2],
    });

    const available = element.getAvailableSelections(result);
    assertEquals(3, available.length);

    assertDeepEquals(
        available[2]!,
        element.getNextSelection(
            result, available[0]!, SelectionDirection.kForward,
            SelectionStep.kWholeLine));
    assertDeepEquals(
        available[0]!,
        element.getNextSelection(
            result, available[2]!, SelectionDirection.kBackward,
            SelectionStep.kWholeLine));
  });

  test('getNextSelection Empty Results Edge Cases', () => {
    const selection = {
      line: 0,
      state: SelectionLineState.kNormal,
      actionIndex: 0,
    };

    // Empty result matches
    const result = createAutocompleteResultForTesting({
      matches: [],
    });
    assertDeepEquals(
        selection,
        element.getNextSelection(
            result, selection, SelectionDirection.kForward,
            SelectionStep.kStateOrLine));

    // Null result
    assertDeepEquals(
        selection,
        element.getNextSelection(
            null, selection, SelectionDirection.kForward,
            SelectionStep.kStateOrLine));
  });

  test('selectionIsNativelySupported', () => {
    assertTrue(selectionIsNativelySupported(
        {line: 0, state: SelectionLineState.kNormal, actionIndex: 0}));
    assertFalse(selectionIsNativelySupported({
      line: 0,
      state: SelectionLineState.kFocusedButtonContextEntrypoint,
      actionIndex: 0,
    }));
  });

  test('instant keyword match selection', () => {
    const normalMatch = createAutocompleteMatch();
    const instantMatch = createAutocompleteMatch();
    instantMatch.keywordModel = createMatchKeywordModelForTesting({
      type: KeywordType.kInstant,
      keyword: '@bookmarks',
      chipHint: 'Bookmarks',
    });

    const result = createAutocompleteResultForTesting({
      matches: [normalMatch, instantMatch],
    });

    const available = element.getAvailableSelections(result);
    assertEquals(2, available.length);
    assertDeepEquals(
        {line: 0, state: SelectionLineState.kNormal, actionIndex: 0},
        available[0]);
    assertDeepEquals(
        {line: 1, state: SelectionLineState.kKeywordMode, actionIndex: 0},
        available[1]);

    // WholeLine stepping navigates directly to kKeywordMode on the instant
    // match.
    const next = element.getNextSelection(
        result, available[0]!, SelectionDirection.kForward,
        SelectionStep.kWholeLine);
    assertDeepEquals(available[1]!, next);

    const prev = element.getNextSelection(
        result, available[1]!, SelectionDirection.kBackward,
        SelectionStep.kWholeLine);
    assertDeepEquals(available[0]!, prev);
  });
});

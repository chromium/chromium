// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {createAutocompleteMatch, createKeywordModelForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {SearchboxSelectionMixin, selectionIsNativelySupported, selectionsEqual, selectionToString} from 'chrome://resources/cr_components/searchbox/searchbox_selection_mixin.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {PageHandlerRemote, SelectionDirection, SelectionLineState, SelectionStep} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

const TestElementBase = SearchboxSelectionMixin(CrLitElement);

class TestSearchboxSelectionMixinElement extends TestElementBase {
  static get is() {
    return 'test-searchbox-selection-mixin';
  }
  isAimVisible: boolean = false;
  showEntrypoint: boolean = false;
  dropdownIsVisible: boolean = true;
  result: any = null;
  mockPageHandler: TestMock<PageHandlerRemote> =
      TestMock.fromClass(PageHandlerRemote);
  mockInputElement: any = {
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
    const result = {
      matches: [match1],
      suggestionGroupsMap: {},
    } as any;

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

  test('getNextSelection Forward Line', () => {
    const match1 = createAutocompleteMatch();
    const match2 = createAutocompleteMatch();
    const match3 = createAutocompleteMatch();
    const result = {
      matches: [match1, match2, match3],
      suggestionGroupsMap: {},
    } as any;

    const available = element.getAvailableSelections(result);
    assertEquals(3, available.length);

    const next = element.getNextSelection(
        result, available[0]!, SelectionDirection.kForward,
        SelectionStep.kStateOrLine);
    assertDeepEquals(available[1]!, next);

    const nextEnd = element.getNextSelection(
        result, available[2]!, SelectionDirection.kForward,
        SelectionStep.kStateOrLine);
    assertDeepEquals(available[0]!, nextEnd);
  });

  test('getNextSelection Backward Line', () => {
    const match1 = createAutocompleteMatch();
    const match2 = createAutocompleteMatch();
    const match3 = createAutocompleteMatch();
    const result = {
      matches: [match1, match2, match3],
      suggestionGroupsMap: {},
    } as any;

    const available = element.getAvailableSelections(result);

    const prev = element.getNextSelection(
        result, available[1]!, SelectionDirection.kBackward,
        SelectionStep.kStateOrLine);
    assertDeepEquals(available[0]!, prev);

    const prevStart = element.getNextSelection(
        result, available[0]!, SelectionDirection.kBackward,
        SelectionStep.kStateOrLine);
    assertDeepEquals(available[2]!, prevStart);
  });

  test('getNextSelection AllLines (PageUp/PageDown)', () => {
    const match1 = createAutocompleteMatch();
    const match2 = createAutocompleteMatch();
    match2.supportsDeletion = true;  // adds a focused button
    const match3 = createAutocompleteMatch();
    const result = {
      matches: [match1, match2, match3],
      suggestionGroupsMap: {},
    } as any;

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
  });

  test('getNextSelection All SelectionLineState types', () => {
    const match = createAutocompleteMatch();
    match.keywordModel =
        createKeywordModelForTesting({chipHint: 'Search keyword'});
    match.actions = [{} as any, {} as any];
    match.supportsDeletion = true;

    const result = {
      matches: [match],
      suggestionGroupsMap: {},
    } as any;

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
    const result = {
      matches: [match1, match2],
      suggestionGroupsMap: {},
    } as any;

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
    const result = {
      matches: [],
      suggestionGroupsMap: {},
    } as any;
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

  test('selectionToString', () => {
    const str = selectionToString(
        {line: 5, state: SelectionLineState.kNormal, actionIndex: 0});
    assertEquals('{5,1,0}', str);
  });
});

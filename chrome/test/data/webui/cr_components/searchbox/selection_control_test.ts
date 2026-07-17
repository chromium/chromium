// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {createAutocompleteMatch} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {getMatchSelections, getNextSelection, selectionIsNativelySupported, selectionsEqual, selectionToString} from 'chrome://resources/cr_components/searchbox/selection_control.js';
import {SelectionDirection, SelectionLineState, SelectionStep} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('CrComponentsSearchboxSelectionControlTest', () => {
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

  test('getMatchSelections', () => {
    const match1 = createAutocompleteMatch();
    const match2 = createAutocompleteMatch();
    const match3 = createAutocompleteMatch();
    match3.supportsDeletion = true;

    const result = {
      matches: [match1, match2, match3],
      suggestionGroupsMap: {},
    } as any;

    const selections = getMatchSelections(result);
    assertEquals(4, selections.length);
    assertDeepEquals(
        {line: 0, state: SelectionLineState.kNormal, actionIndex: 0},
        selections[0]);
    assertDeepEquals(
        {line: 1, state: SelectionLineState.kNormal, actionIndex: 0},
        selections[1]);
    assertDeepEquals(
        {line: 2, state: SelectionLineState.kNormal, actionIndex: 0},
        selections[2]);
    assertDeepEquals(
        {
          line: 2,
          state: SelectionLineState.kFocusedButtonRemoveSuggestion,
          actionIndex: 0,
        },
        selections[3]);
  });

  test('getNextSelection Forward Line', () => {
    const available = [
      {line: 0, state: SelectionLineState.kNormal, actionIndex: 0},
      {line: 1, state: SelectionLineState.kNormal, actionIndex: 0},
      {line: 2, state: SelectionLineState.kNormal, actionIndex: 0},
    ];

    const next = getNextSelection(
        available[0]!, SelectionDirection.kForward, SelectionStep.kStateOrLine,
        available);
    assertDeepEquals(available[1]!, next);

    const nextEnd = getNextSelection(
        available[2]!, SelectionDirection.kForward, SelectionStep.kStateOrLine,
        available);
    assertDeepEquals(available[0]!, nextEnd);
  });

  test('getNextSelection Backward Line', () => {
    const available = [
      {line: 0, state: SelectionLineState.kNormal, actionIndex: 0},
      {line: 1, state: SelectionLineState.kNormal, actionIndex: 0},
      {line: 2, state: SelectionLineState.kNormal, actionIndex: 0},
    ];

    const prev = getNextSelection(
        available[1]!, SelectionDirection.kBackward, SelectionStep.kStateOrLine,
        available);
    assertDeepEquals(available[0]!, prev);

    const prevStart = getNextSelection(
        available[0]!, SelectionDirection.kBackward, SelectionStep.kStateOrLine,
        available);
    assertDeepEquals(available[2]!, prevStart);
  });

  test('getNextSelection AllLines (PageUp/PageDown)', () => {
    const available = [
      {line: 0, state: SelectionLineState.kNormal, actionIndex: 0},
      {line: 1, state: SelectionLineState.kNormal, actionIndex: 0},
      {
        line: 1,
        state: SelectionLineState.kFocusedButtonRemoveSuggestion,
        actionIndex: 0,
      },
      {line: 2, state: SelectionLineState.kNormal, actionIndex: 0},
    ];

    // PageDown (Forward, AllLines) should go to the very last normal match.
    const pageDown = getNextSelection(
        available[0]!, SelectionDirection.kForward, SelectionStep.kAllLines,
        available);
    assertDeepEquals(available[3]!, pageDown);

    // PageUp (Backward, AllLines) should go to the very first normal match.
    const pageUp = getNextSelection(
        available[3]!, SelectionDirection.kBackward, SelectionStep.kAllLines,
        available);
    assertDeepEquals(available[0]!, pageUp);
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

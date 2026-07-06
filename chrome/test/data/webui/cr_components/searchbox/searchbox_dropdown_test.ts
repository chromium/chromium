// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/searchbox/searchbox_dropdown.js';
import 'chrome://new-tab-page/strings.m.js';

import {SelectionDirection, SelectionLineState, SelectionStep} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {SearchboxDropdownElement} from 'chrome://resources/cr_components/searchbox/searchbox_dropdown.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, microtasksFinished} from 'chrome://webui-test/test_util.js';

// TODO(crbug.com/455876602): Move dropdown-specific tests in searchbox_test.ts
//  into this file.
suite('SearchboxDropdown', () => {
  let dropdown: SearchboxDropdownElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dropdown = document.createElement('cr-searchbox-dropdown');
    document.body.appendChild(dropdown);
  });

  test('hides matches that have `isHidden` field set to true', async () => {
    // Arrange.
    const matches = [
      createSearchMatchForTesting({contents: 'bar', isHidden: true}),
      createSearchMatchForTesting({contents: 'foo'}),
    ];

    // Act.
    dropdown.result = createAutocompleteResultForTesting({matches});
    await microtasksFinished();

    // Assert.
    const matchEls = dropdown.shadowRoot.querySelectorAll('cr-searchbox-match');
    assertEquals(1, matchEls.length);
    const contentsEl = $$(matchEls[0]!, '#contents');
    assertTrue(!!contentsEl);
    assertEquals('foo', contentsEl.textContent.trim());
    // The visible element's matchIndex must retain its original index (1),
    // even though it is the first (and only) visible element.
    assertEquals(1, matchEls[0]!.matchIndex);
  });

  test(
      'stepSelection: forward navigation through SelectionLineState types',
      async () => {
        const matches = [
          createSearchMatchForTesting({
            keywordChipHint: 'Search keyword',
            actions: [{id: 1} as any],
            supportsDeletion: true,
          }),
        ];
        dropdown.result = createAutocompleteResultForTesting({matches});
        await microtasksFinished();

        // Initially at kDefaultSelection (line: -1, state: kNormal,
        // actionIndex: 0).
        // 1. First step forward -> line 0, state kNormal.
        dropdown.stepSelection(
            SelectionDirection.kForward, SelectionStep.kStateOrLine);
        assertEquals(0, dropdown.selection.line);
        assertEquals(SelectionLineState.kNormal, dropdown.selection.state);

        // 2. Second step forward -> line 0, state kKeywordMode.
        dropdown.stepSelection(
            SelectionDirection.kForward, SelectionStep.kStateOrLine);
        assertEquals(0, dropdown.selection.line);
        assertEquals(SelectionLineState.kKeywordMode, dropdown.selection.state);

        // 3. Third step forward -> line 0, state kFocusedButtonAction.
        dropdown.stepSelection(
            SelectionDirection.kForward, SelectionStep.kStateOrLine);
        assertEquals(0, dropdown.selection.line);
        assertEquals(
            SelectionLineState.kFocusedButtonAction, dropdown.selection.state);
        assertEquals(0, dropdown.selection.actionIndex);

        // 4. Fourth step forward -> line 0, state
        // kFocusedButtonRemoveSuggestion.
        dropdown.stepSelection(
            SelectionDirection.kForward, SelectionStep.kStateOrLine);
        assertEquals(0, dropdown.selection.line);
        assertEquals(
            SelectionLineState.kFocusedButtonRemoveSuggestion,
            dropdown.selection.state);

        // 5. Fifth step forward -> wraps back to line 0, state kNormal.
        dropdown.stepSelection(
            SelectionDirection.kForward, SelectionStep.kStateOrLine);
        assertEquals(0, dropdown.selection.line);
        assertEquals(SelectionLineState.kNormal, dropdown.selection.state);
      });

  test(
      'stepSelection: backward navigation through SelectionLineState types',
      async () => {
        const matches = [
          createSearchMatchForTesting({
            keywordChipHint: 'Search keyword',
            actions: [{id: 1} as any],
            supportsDeletion: true,
          }),
        ];
        dropdown.result = createAutocompleteResultForTesting({matches});
        await microtasksFinished();

        // Initially at kDefaultSelection (line: -1, state: kNormal,
        // actionIndex: 0).
        // 1. First step backward -> wraps to the last possible state: line 0,
        // state kFocusedButtonRemoveSuggestion.
        dropdown.stepSelection(
            SelectionDirection.kBackward, SelectionStep.kStateOrLine);
        assertEquals(0, dropdown.selection.line);
        assertEquals(
            SelectionLineState.kFocusedButtonRemoveSuggestion,
            dropdown.selection.state);

        // 2. Second step backward -> line 0, state kFocusedButtonAction.
        dropdown.stepSelection(
            SelectionDirection.kBackward, SelectionStep.kStateOrLine);
        assertEquals(0, dropdown.selection.line);
        assertEquals(
            SelectionLineState.kFocusedButtonAction, dropdown.selection.state);

        // 3. Third step backward -> line 0, state kKeywordMode.
        dropdown.stepSelection(
            SelectionDirection.kBackward, SelectionStep.kStateOrLine);
        assertEquals(0, dropdown.selection.line);
        assertEquals(SelectionLineState.kKeywordMode, dropdown.selection.state);

        // 4. Fourth step backward -> line 0, state kNormal.
        dropdown.stepSelection(
            SelectionDirection.kBackward, SelectionStep.kStateOrLine);
        assertEquals(0, dropdown.selection.line);
        assertEquals(SelectionLineState.kNormal, dropdown.selection.state);

        // 5. Fifth step backward -> wraps to the last possible state: line 0,
        // state kFocusedButtonRemoveSuggestion.
        dropdown.stepSelection(
            SelectionDirection.kBackward, SelectionStep.kStateOrLine);
        assertEquals(0, dropdown.selection.line);
        assertEquals(
            SelectionLineState.kFocusedButtonRemoveSuggestion,
            dropdown.selection.state);
      });

  test('stepSelection: multiple matches forward and backward', async () => {
    const matches = [
      createSearchMatchForTesting({
        keywordChipHint: 'Keyword 0',
      }),
      createSearchMatchForTesting({
        supportsDeletion: true,
      }),
    ];
    dropdown.result = createAutocompleteResultForTesting({matches});
    await microtasksFinished();

    // Initial selection is default.
    // Step forward: Line 0, kNormal.
    dropdown.stepSelection(
        SelectionDirection.kForward, SelectionStep.kStateOrLine);
    assertEquals(0, dropdown.selection.line);
    assertEquals(SelectionLineState.kNormal, dropdown.selection.state);

    // Step forward: Line 0, kKeywordMode.
    dropdown.stepSelection(
        SelectionDirection.kForward, SelectionStep.kStateOrLine);
    assertEquals(0, dropdown.selection.line);
    assertEquals(SelectionLineState.kKeywordMode, dropdown.selection.state);

    // Step forward: Line 1, kNormal.
    dropdown.stepSelection(
        SelectionDirection.kForward, SelectionStep.kStateOrLine);
    assertEquals(1, dropdown.selection.line);
    assertEquals(SelectionLineState.kNormal, dropdown.selection.state);

    // Step forward: Line 1, kFocusedButtonRemoveSuggestion.
    dropdown.stepSelection(
        SelectionDirection.kForward, SelectionStep.kStateOrLine);
    assertEquals(1, dropdown.selection.line);
    assertEquals(
        SelectionLineState.kFocusedButtonRemoveSuggestion,
        dropdown.selection.state);

    // Step forward: Line 0, kNormal.
    dropdown.stepSelection(
        SelectionDirection.kForward, SelectionStep.kStateOrLine);
    assertEquals(0, dropdown.selection.line);
    assertEquals(SelectionLineState.kNormal, dropdown.selection.state);

    // Step backward: Line 1, kFocusedButtonRemoveSuggestion.
    dropdown.stepSelection(
        SelectionDirection.kBackward, SelectionStep.kStateOrLine);
    assertEquals(1, dropdown.selection.line);
    assertEquals(
        SelectionLineState.kFocusedButtonRemoveSuggestion,
        dropdown.selection.state);
  });

  test('stepSelection: edge case empty results', async () => {
    dropdown.result = createAutocompleteResultForTesting({matches: []});
    await microtasksFinished();

    // Calling stepSelection with empty results should not change selection or
    // crash.
    const initialSelection = dropdown.selection;
    dropdown.stepSelection(
        SelectionDirection.kForward, SelectionStep.kStateOrLine);
    assertEquals(initialSelection.line, dropdown.selection.line);
    assertEquals(initialSelection.state, dropdown.selection.state);
  });

  test('stepSelection: SelectionStep.kWholeLine', async () => {
    const matches = [
      createSearchMatchForTesting({
        keywordChipHint: 'Keyword 0',
      }),
      createSearchMatchForTesting({
        supportsDeletion: true,
      }),
    ];
    dropdown.result = createAutocompleteResultForTesting({matches});
    await microtasksFinished();

    // Start at line 0, kNormal.
    dropdown.stepSelection(
        SelectionDirection.kForward, SelectionStep.kStateOrLine);
    assertEquals(0, dropdown.selection.line);
    assertEquals(SelectionLineState.kNormal, dropdown.selection.state);

    // Step SelectionStep.kWholeLine forward -> goes to Line 1, kNormal (skips
    // Line 0 kKeywordMode).
    dropdown.stepSelection(
        SelectionDirection.kForward, SelectionStep.kWholeLine);
    assertEquals(1, dropdown.selection.line);
    assertEquals(SelectionLineState.kNormal, dropdown.selection.state);

    // Step SelectionStep.kWholeLine forward -> wraps to Line 0, kNormal (skips
    // Line 1 kFocusedButtonRemoveSuggestion).
    dropdown.stepSelection(
        SelectionDirection.kForward, SelectionStep.kWholeLine);
    assertEquals(0, dropdown.selection.line);
    assertEquals(SelectionLineState.kNormal, dropdown.selection.state);

    // Step SelectionStep.kWholeLine backward -> wraps to Line 1, kNormal.
    dropdown.stepSelection(
        SelectionDirection.kBackward, SelectionStep.kWholeLine);
    assertEquals(1, dropdown.selection.line);
    assertEquals(SelectionLineState.kNormal, dropdown.selection.state);
  });

  test('stepSelection: SelectionStep.kAllLines', async () => {
    const matches = [
      createSearchMatchForTesting(),
      createSearchMatchForTesting(),
      createSearchMatchForTesting(),
    ];
    dropdown.result = createAutocompleteResultForTesting({matches});
    await microtasksFinished();

    // Step kAllLines forward -> goes to last line, kNormal.
    dropdown.stepSelection(
        SelectionDirection.kForward, SelectionStep.kAllLines);
    assertEquals(2, dropdown.selection.line);
    assertEquals(SelectionLineState.kNormal, dropdown.selection.state);

    // Step kAllLines backward -> goes to first line, kNormal.
    dropdown.stepSelection(
        SelectionDirection.kBackward, SelectionStep.kAllLines);
    assertEquals(0, dropdown.selection.line);
    assertEquals(SelectionLineState.kNormal, dropdown.selection.state);
  });
});

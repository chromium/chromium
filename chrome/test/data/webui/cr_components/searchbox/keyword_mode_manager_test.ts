// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {KeywordModeManager} from '//resources/cr_components/searchbox/keyword_mode_manager.js';
import type {KeywordClearedEvent} from '//resources/cr_components/searchbox/keyword_mode_manager.js';
import {createMatchKeywordModelForTesting, createSearchMatchForTesting} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {KeywordType} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('KeywordModeManagerTest', () => {
  let manager: KeywordModeManager;
  let modelChangedCount: number;
  let lastKeywordCleared: KeywordClearedEvent|null;
  let keywordEnteredCount: number;

  setup(() => {
    modelChangedCount = 0;
    lastKeywordCleared = null;
    keywordEnteredCount = 0;
    manager = new KeywordModeManager({
      onKeywordModelChanged: () => {
        modelChangedCount++;
      },
      onKeywordCleared: (e) => {
        lastKeywordCleared = e;
      },
      onKeywordEntered: () => {
        keywordEnteredCount++;
      },
    });
  });

  test('initial state', () => {
    assertEquals(null, manager.inputKeywordModel);
    assertFalse(manager.isInKeywordMode);
    assertEquals('', manager.activeKeyword);
  });

  test('enter and exit keyword mode', () => {
    manager.enter('google.com', 'Search Google');
    assertEquals(1, modelChangedCount);
    assertTrue(manager.isInKeywordMode);
    assertEquals('google.com', manager.activeKeyword);
    assertEquals(KeywordType.kInKeyword, manager.inputKeywordModel?.type);
    assertEquals('google.com', manager.inputKeywordModel?.keyword);
    assertEquals('Search Google', manager.inputKeywordModel?.displayText);

    manager.exit();
    assertEquals(2, modelChangedCount);
    assertFalse(manager.isInKeywordMode);
    assertEquals('', manager.activeKeyword);
    assertEquals(null, manager.inputKeywordModel);
  });

  test('acceptInputTrigger for space at end', () => {
    // Null cursor position -> false.
    assertFalse(manager.acceptInputTrigger('google.com ', null));

    // No keyword model -> false.
    assertFalse(manager.acceptInputTrigger('google.com ', 11));

    // Keyword chip shown.
    manager.inputKeywordModel = {
      type: KeywordType.kChip,
      keyword: 'google.com',
      displayText: 'Search Google',
    };

    // Cursor not at end -> false.
    assertFalse(manager.acceptInputTrigger('google.com ', 5));

    // Input does not end with space -> false.
    assertFalse(manager.acceptInputTrigger('google.com', 10));

    // Input does not match keyword -> false.
    assertFalse(manager.acceptInputTrigger('yahoo.com ', 10));

    // Already in keyword mode (kInKeyword) -> false.
    manager.inputKeywordModel = {
      type: KeywordType.kInKeyword,
      keyword: 'google.com',
      displayText: 'Search Google',
    };
    assertFalse(manager.acceptInputTrigger('google.com ', 11));

    // Correct input and cursor at end -> true and automatically enters keyword
    // mode.
    manager.inputKeywordModel = {
      type: KeywordType.kChip,
      keyword: 'google.com',
      displayText: 'Search Google',
    };
    assertTrue(manager.acceptInputTrigger('google.com ', 11));
    assertTrue(manager.isInKeywordMode);
    assertEquals('google.com', manager.activeKeyword);

    // Ideographic space at end -> true.
    manager.inputKeywordModel = {
      type: KeywordType.kChip,
      keyword: 'google.com',
      displayText: 'Search Google',
    };
    assertTrue(manager.acceptInputTrigger('google.com　', 11));
    assertTrue(manager.isInKeywordMode);
  });

  test('acceptInputTrigger for question mark', () => {
    // Null cursor position -> false.
    assertFalse(manager.acceptInputTrigger('?', null));

    // Input not '?' -> false.
    assertFalse(manager.acceptInputTrigger('?a', 2));
    assertFalse(manager.acceptInputTrigger('hello', 5));

    // Cursor not at 1 -> false.
    assertFalse(manager.acceptInputTrigger('?', 0));

    // Already in keyword mode -> false.
    manager.enter('google.com', 'Search Google');
    assertFalse(manager.acceptInputTrigger('?', 1));

    // Input '?' with cursor at 1 -> true and automatically enters question mark
    // keyword mode.
    manager.exit();
    assertTrue(manager.acceptInputTrigger('?', 1));
    assertTrue(manager.isInKeywordMode);
    assertEquals('?', manager.activeKeyword);
    assertEquals('', manager.inputKeywordModel?.displayText);
  });

  test('handleBackspace', () => {
    // Not in keyword mode -> returns false.
    const inputState = {
      value: 'query',
      selectionStart: 0,
      selectionEnd: 0,
    };
    assertFalse(manager.handleBackspace(inputState));

    // In keyword mode, but cursor not at 0 -> returns false.
    manager.enter('google.com', 'Search Google');
    assertFalse(manager.handleBackspace({
      value: 'query',
      selectionStart: 2,
      selectionEnd: 2,
    }));

    // In keyword mode with cursor at 0 -> exits keyword mode and emits
    // onKeywordCleared.
    assertTrue(manager.handleBackspace({
      value: 'query',
      selectionStart: 0,
      selectionEnd: 0,
    }));
    assertFalse(manager.isInKeywordMode);
    assertEquals('google.com query', lastKeywordCleared?.restoredText);
    assertEquals(11, lastKeywordCleared?.cursorPosition);
  });

  test('handleKeywordClick', () => {
    const matchWithoutKeyword = createSearchMatchForTesting({
      keywordModel: undefined,
    });
    assertThrows(() => manager.handleKeywordClick(matchWithoutKeyword));

    const matchWithKeyword = createSearchMatchForTesting({
      keywordModel: createMatchKeywordModelForTesting({
        type: KeywordType.kChip,
        keyword: 'youtube.com',
        chipHint: 'Search YouTube',
      }),
    });

    manager.handleKeywordClick(matchWithKeyword);
    assertTrue(manager.isInKeywordMode);
    assertEquals('youtube.com', manager.activeKeyword);
    assertEquals(1, keywordEnteredCount);
  });

  test('acceptTab', () => {
    const matchWithoutKeyword = createSearchMatchForTesting({
      allowedToBeDefaultMatch: true,
      keywordModel: undefined,
    });
    const matchWithKeyword = createSearchMatchForTesting({
      allowedToBeDefaultMatch: true,
      keywordModel: createMatchKeywordModelForTesting({
        type: KeywordType.kChip,
        keyword: 'youtube.com',
        chipHint: 'Search YouTube',
      }),
    });
    const matchWithKeywordNotAllowedDefault = createSearchMatchForTesting({
      allowedToBeDefaultMatch: false,
      keywordModel: createMatchKeywordModelForTesting({
        type: KeywordType.kChip,
        keyword: 'youtube.com',
        chipHint: 'Search YouTube',
      }),
    });

    // Null match -> false.
    assertFalse(manager.acceptTab(null, /*matchIndex=*/ 0));

    // Match without keyword -> false.
    assertFalse(manager.acceptTab(matchWithoutKeyword, /*matchIndex=*/ 0));

    // Non-default matchIndex with keyword -> false.
    assertFalse(manager.acceptTab(matchWithKeyword, /*matchIndex=*/ 1));

    // Match not allowed to be default -> false.
    assertFalse(manager.acceptTab(
        matchWithKeywordNotAllowedDefault, /*matchIndex=*/ 0));

    // Default match with keyword -> enters keyword mode and notifies delegate.
    assertTrue(manager.acceptTab(matchWithKeyword, /*matchIndex=*/ 0));
    assertTrue(manager.isInKeywordMode);
    assertEquals('youtube.com', manager.activeKeyword);
    assertEquals(1, keywordEnteredCount);
  });

  test('formatMatchFillIntoEdit', () => {
    const match = createSearchMatchForTesting({
      fillIntoEdit: 'google.com chromium news',
    });

    // Not in keyword mode -> returns original fillIntoEdit.
    assertEquals(
        'google.com chromium news',
        manager.formatMatchFillIntoEdit(match, /*matchIndex=*/ 0));

    // In keyword mode with matching keyword prefix -> strips prefix and
    // trailing space.
    manager.enter('google.com', 'Search Google');
    assertEquals(
        'chromium news',
        manager.formatMatchFillIntoEdit(match, /*matchIndex=*/ 0));

    // In keyword mode with non-matching fillIntoEdit -> returns original.
    const otherMatch = createSearchMatchForTesting({
      fillIntoEdit: 'other fill',
    });
    assertEquals(
        'other fill',
        manager.formatMatchFillIntoEdit(otherMatch, /*matchIndex=*/ 1));

    // Default match with lastQueriedInput -> restores lastQueriedInput +
    // inlineAutocompletion.
    const urlMatch = createSearchMatchForTesting({
      fillIntoEdit: 'https://chromium.org/',
      inlineAutocompletion: 'ium.org/',
      allowedToBeDefaultMatch: true,
    });
    assertEquals(
        'chromium.org/',
        manager.formatMatchFillIntoEdit(
            urlMatch, /*matchIndex=*/ 0, /*lastQueriedInput=*/ 'chrom'));
  });

  test('onSelectedMatchChanged', () => {
    const matchWithKeyword = createSearchMatchForTesting({
      keywordModel: createMatchKeywordModelForTesting({
        type: KeywordType.kChip,
        keyword: 'youtube.com',
        chipHint: 'Search YouTube',
      }),
    });

    manager.onSelectedMatchChanged(matchWithKeyword);
    assertTrue(manager.inputKeywordModel !== null);
    assertEquals(KeywordType.kChip, manager.inputKeywordModel?.type);
    assertEquals('youtube.com', manager.inputKeywordModel?.keyword);
    assertEquals('Search YouTube', manager.inputKeywordModel?.displayText);

    // Match without keyword model -> resets keyword model to null.
    const matchWithoutKeyword = createSearchMatchForTesting({
      keywordModel: undefined,
    });
    manager.onSelectedMatchChanged(matchWithoutKeyword);
    assertEquals(null, manager.inputKeywordModel);

    // In keyword mode with null match (e.g. results clearing) -> preserves
    // keyword model.
    manager.enter('youtube.com', 'Search YouTube');
    manager.onSelectedMatchChanged(null);
    assertTrue(manager.inputKeywordModel !== null);
    assertEquals(KeywordType.kInKeyword, manager.inputKeywordModel?.type);
    assertEquals('youtube.com', manager.inputKeywordModel?.keyword);
  });
});

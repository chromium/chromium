// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/strings.m.js';

import {KeywordModeEntryMethod, KeywordModeManager} from '//resources/cr_components/searchbox/keyword_mode_manager.js';
import type {KeywordClearedEvent} from '//resources/cr_components/searchbox/keyword_mode_manager.js';
import {createMatchKeywordModelForTesting, createSearchMatchForTesting} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {KeywordType, SelectionLineState} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('KeywordModeManagerTest', () => {
  let manager: KeywordModeManager;
  let modelChangedCount: number;
  let lastKeywordCleared: KeywordClearedEvent|null;
  let keywordEnteredCount: number;

  const spaceEvent = new KeyboardEvent('keydown', {key: ' '});
  const ideographicSpaceEvent = new KeyboardEvent('keydown', {key: '\u3000'});
  const backspaceEvent = new KeyboardEvent('keydown', {key: 'Backspace'});
  const questionMarkEvent = new KeyboardEvent('keydown', {key: '?'});
  const spaceInputEvent =
      new InputEvent('input', {inputType: 'insertText', data: ' '});

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

  teardown(() => {
    if (loadTimeData.isInitialized()) {
      loadTimeData.overrideValues({keywordSpaceTriggeringEnabled: true});
    }
  });

  test('initial state', () => {
    assertEquals(null, manager.inputKeywordModel);
    assertFalse(manager.isInKeywordMode);
    assertEquals('', manager.activeKeyword);
  });

  test('enter and exit keyword mode', () => {
    manager.enter('google.com', 'Search Google', KeywordModeEntryMethod.TAB);
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
    assertFalse(manager.acceptInputTrigger('google.com ', null, spaceEvent));

    // No keyword model -> false.
    assertFalse(manager.acceptInputTrigger('google.com ', 11, spaceEvent));

    // Keyword chip shown.
    manager.inputKeywordModel = {
      type: KeywordType.kChip,
      keyword: 'google.com',
      displayText: 'Search Google',
    };

    // Cursor not at end -> false.
    assertFalse(manager.acceptInputTrigger('google.com ', 5, spaceEvent));

    // Input does not end with space -> false.
    assertFalse(manager.acceptInputTrigger('google.com', 10, spaceEvent));

    // Input does not match keyword -> false.
    assertFalse(manager.acceptInputTrigger('yahoo.com ', 10, spaceEvent));

    // Already in keyword mode (kInKeyword) -> false.
    manager.inputKeywordModel = {
      type: KeywordType.kInKeyword,
      keyword: 'google.com',
      displayText: 'Search Google',
    };
    assertFalse(manager.acceptInputTrigger('google.com ', 11, spaceEvent));

    // Correct input and cursor at end -> true and automatically enters keyword
    // mode with SPACE_AT_END entry method.
    manager.inputKeywordModel = {
      type: KeywordType.kChip,
      keyword: 'google.com',
      displayText: 'Search Google',
    };
    assertTrue(manager.acceptInputTrigger('google.com ', 11, spaceEvent));
    assertTrue(manager.isInKeywordMode);
    assertEquals('google.com', manager.activeKeyword);

    // Ideographic space at end -> true.
    manager.inputKeywordModel = {
      type: KeywordType.kChip,
      keyword: 'google.com',
      displayText: 'Search Google',
    };
    assertTrue(manager.acceptInputTrigger(
        'google.com\u3000', 11, ideographicSpaceEvent));
    assertTrue(manager.isInKeywordMode);

    // Case-insensitive match at end -> true.
    manager.exit();
    manager.inputKeywordModel = {
      type: KeywordType.kChip,
      keyword: 'google.com',
      displayText: 'Search Google',
    };
    assertTrue(manager.acceptInputTrigger('GOOGLE.COM ', 11, spaceEvent));
    assertTrue(manager.isInKeywordMode);
    assertEquals('google.com', manager.activeKeyword);

    // Starter pack keyword in availableKeywordModels -> true on space at end.
    manager.exit();
    manager.availableKeywordModels = [
      {
        type: KeywordType.kInstant,
        keyword: '@history',
        displayText: '@history',
      },
      {
        type: KeywordType.kChip,
        keyword: 'google.com',
        displayText: 'Search Google',
      },
    ];
    assertTrue(manager.acceptInputTrigger('@history ', 9, spaceEvent));
    assertTrue(manager.isInKeywordMode);
    assertEquals('@history', manager.activeKeyword);
    assertEquals('@history', manager.inputKeywordModel?.displayText);
    assertEquals(KeywordModeEntryMethod.SPACE_AT_END, manager.entryMethod);
    manager.exit();

    // Starter pack keyword case-insensitive -> true.
    assertTrue(manager.acceptInputTrigger('@HISTORY ', 9, spaceEvent));
    assertTrue(manager.isInKeywordMode);
    assertEquals('@history', manager.activeKeyword);
    manager.exit();

    // Starter pack keyword with ideographic space -> true.
    assertTrue(
        manager.acceptInputTrigger('@history\u3000', 9, ideographicSpaceEvent));
    assertTrue(manager.isInKeywordMode);
    assertEquals('@history', manager.activeKeyword);
    manager.exit();

    // Starter pack keyword in inputKeywordModel -> true.
    manager.availableKeywordModels = [];
    manager.inputKeywordModel = {
      type: KeywordType.kInstant,
      keyword: '@bookmarks',
      displayText: '@bookmarks',
    };
    assertTrue(manager.acceptInputTrigger('@bookmarks ', 11, spaceEvent));
    assertTrue(manager.isInKeywordMode);
    assertEquals('@bookmarks', manager.activeKeyword);
    manager.exit();

    // Available keyword in availableKeywordModels without chip shown -> true.
    manager.availableKeywordModels = [{
      type: KeywordType.kChip,
      keyword: 'google.com',
      displayText: 'Search Google',
    }];
    assertTrue(manager.acceptInputTrigger('google.com ', 11, spaceEvent));
    assertTrue(manager.isInKeywordMode);
    assertEquals('google.com', manager.activeKeyword);
    assertEquals('Search Google', manager.inputKeywordModel?.displayText);
    assertEquals(KeywordModeEntryMethod.SPACE_AT_END, manager.entryMethod);
    manager.exit();

    // Keyword not in availableKeywordModels without chip shown -> false.
    assertFalse(manager.acceptInputTrigger('yahoo.com ', 10, spaceEvent));
    assertFalse(manager.isInKeywordMode);

    // Space was not typed (e.g. deleting trailing space from "@history  ") ->
    // false.
    manager.availableKeywordModels = [{
      type: KeywordType.kInstant,
      keyword: '@history',
      displayText: '@history',
    }];
    assertFalse(manager.acceptInputTrigger('@history ', 9, backspaceEvent));
    assertFalse(manager.isInKeywordMode);

    const backspaceInputEvent =
        new InputEvent('input', {inputType: 'deleteContentBackward'});
    assertFalse(
        manager.acceptInputTrigger('@history ', 9, backspaceInputEvent));
    assertFalse(manager.isInKeywordMode);

    const pasteInputEvent =
        new InputEvent('input', {inputType: 'insertFromPaste', data: ' '});
    assertFalse(manager.acceptInputTrigger('@history ', 9, pasteInputEvent));
    assertFalse(manager.isInKeywordMode);

    // Starter pack keyword when keywordSpaceTriggeringEnabled is false ->
    // false.
    manager.availableKeywordModels = [{
      type: KeywordType.kInstant,
      keyword: '@history',
      displayText: '@history',
    }];
    manager.keywordSpaceTriggeringEnabled = false;
    assertFalse(manager.acceptInputTrigger('@history ', 9, spaceEvent));
    assertFalse(manager.isInKeywordMode);
    manager.keywordSpaceTriggeringEnabled = true;

    // When keywordSpaceTriggeringEnabled is false -> false.
    manager.exit();
    manager.keywordSpaceTriggeringEnabled = false;
    manager.inputKeywordModel = {
      type: KeywordType.kChip,
      keyword: 'google.com',
      displayText: 'Search Google',
    };
    assertFalse(manager.acceptInputTrigger('google.com ', 11, spaceEvent));
    assertFalse(manager.isInKeywordMode);

    // Reset keywordSpaceTriggeringEnabled.
    manager.keywordSpaceTriggeringEnabled = true;

    // Check constructor initialization from loadTimeData.
    loadTimeData.overrideValues({keywordSpaceTriggeringEnabled: false});
    const disabledManager = new KeywordModeManager({
      onKeywordModelChanged: () => {},
      onKeywordCleared: () => {},
      onKeywordEntered: () => {},
    });
    assertFalse(disabledManager.keywordSpaceTriggeringEnabled);
    loadTimeData.overrideValues({keywordSpaceTriggeringEnabled: true});
  });

  test('acceptInputTrigger for space in middle', () => {
    // Null cursor position -> false.
    assertFalse(
        manager.acceptInputTrigger('google.com query', null, spaceEvent));

    // Non-keyword (no keyword model / not in availableKeywordModels) -> false.
    assertFalse(manager.acceptInputTrigger('cat dog', 4, spaceEvent));
    assertFalse(manager.isInKeywordMode);

    // Provide available keyword models.
    manager.availableKeywordModels = [
      {
        type: KeywordType.kChip,
        keyword: 'google.com',
        displayText: 'Search Google',
      },
      {
        type: KeywordType.kInstant,
        keyword: '@history',
        displayText: '@history',
      },
    ];

    // Keyword in availableKeywordModels -> true.
    assertTrue(manager.acceptInputTrigger('google.com query', 11, spaceEvent));
    assertTrue(manager.isInKeywordMode);
    assertEquals('google.com', manager.activeKeyword);
    assertEquals('Search Google', manager.inputKeywordModel?.displayText);
    manager.exit();

    // InputEvent also works.
    assertTrue(
        manager.acceptInputTrigger('google.com query', 11, spaceInputEvent));
    manager.exit();

    // Starter pack / instant keyword in availableKeywordModels -> true.
    assertTrue(manager.acceptInputTrigger('@history query', 9, spaceEvent));
    assertTrue(manager.isInKeywordMode);
    assertEquals('@history', manager.activeKeyword);
    assertEquals('@history', manager.inputKeywordModel?.displayText);
    manager.exit();

    // Space was not typed (e.g. backspace) -> false.
    assertFalse(
        manager.acceptInputTrigger('google.com query', 11, backspaceEvent));
    assertFalse(manager.isInKeywordMode);

    // Non-keyword (not in availableKeywordModels) -> false.
    assertFalse(manager.acceptInputTrigger('yahoo.com query', 10, spaceEvent));
    assertFalse(manager.isInKeywordMode);

    // When keyword is removed from availableKeywordModels -> false.
    manager.availableKeywordModels = [{
      type: KeywordType.kInstant,
      keyword: '@history',
      displayText: '@history',
    }];
    assertFalse(manager.acceptInputTrigger('google.com query', 11, spaceEvent));
    assertFalse(manager.isInKeywordMode);

    // Restore google.com to availableKeywordModels.
    manager.availableKeywordModels = [
      {
        type: KeywordType.kChip,
        keyword: 'google.com',
        displayText: 'Search Google',
      },
      {
        type: KeywordType.kInstant,
        keyword: '@history',
        displayText: '@history',
      },
    ];

    // Cursor position not immediately after space -> false.
    assertFalse(manager.acceptInputTrigger('google.com query', 10, spaceEvent));
    assertFalse(manager.acceptInputTrigger('google.com query', 16, spaceEvent));

    // Character after keyword is not space -> false.
    assertFalse(manager.acceptInputTrigger('google.com-query', 11, spaceEvent));

    // Text after space starts with another space -> false.
    assertFalse(
        manager.acceptInputTrigger('google.com  query', 11, spaceEvent));

    // Already in keyword mode (kInKeyword) -> false.
    manager.enter('google.com', 'Search Google', KeywordModeEntryMethod.TAB);
    assertFalse(manager.acceptInputTrigger('google.com query', 11, spaceEvent));
    manager.exit();

    // Ideographic space in middle -> true.
    assertTrue(manager.acceptInputTrigger(
        'google.com\u3000query', 11, ideographicSpaceEvent));
    assertTrue(manager.isInKeywordMode);
    assertEquals('google.com', manager.activeKeyword);
    manager.exit();

    // Case-insensitive space in middle -> true.
    assertTrue(manager.acceptInputTrigger('GOOGLE.COM query', 11, spaceEvent));
    assertTrue(manager.isInKeywordMode);
    assertEquals('google.com', manager.activeKeyword);
    manager.exit();

    assertTrue(manager.acceptInputTrigger('Google.com query', 11, spaceEvent));
    assertTrue(manager.isInKeywordMode);
    assertEquals('google.com', manager.activeKeyword);
    manager.exit();

    // Available keyword model with mixed-case keyword -> true.
    manager.availableKeywordModels = [
      {
        type: KeywordType.kChip,
        keyword: 'YouTube.com',
        displayText: 'Search YouTube',
      },
    ];
    assertTrue(manager.acceptInputTrigger('youtube.com query', 12, spaceEvent));
    assertTrue(manager.isInKeywordMode);
    assertEquals('YouTube.com', manager.activeKeyword);
    manager.exit();

    assertTrue(manager.acceptInputTrigger('YOUTUBE.COM query', 12, spaceEvent));
    assertTrue(manager.isInKeywordMode);
    assertEquals('YouTube.com', manager.activeKeyword);
    manager.exit();

    // When keywordSpaceTriggeringEnabled is false -> false.
    manager.keywordSpaceTriggeringEnabled = false;
    assertFalse(manager.acceptInputTrigger('google.com query', 11, spaceEvent));
    assertFalse(manager.isInKeywordMode);
    manager.keywordSpaceTriggeringEnabled = true;
  });

  test('acceptInputTrigger for question mark', () => {
    // Null cursor position -> false.
    assertFalse(manager.acceptInputTrigger('?', null, questionMarkEvent));

    // Input not '?' -> false.
    assertFalse(manager.acceptInputTrigger('?a', 2, questionMarkEvent));
    assertFalse(manager.acceptInputTrigger('hello', 5, questionMarkEvent));

    // Cursor not at 1 -> false.
    assertFalse(manager.acceptInputTrigger('?', 0, questionMarkEvent));

    // Already in keyword mode -> false.
    manager.enter('google.com', 'Search Google', KeywordModeEntryMethod.TAB);
    assertFalse(manager.acceptInputTrigger('?', 1, questionMarkEvent));

    // Input '?' with cursor at 1 -> true and automatically enters question mark
    // keyword mode with QUESTION_MARK entry method.
    manager.exit();
    assertTrue(manager.acceptInputTrigger('?', 1, questionMarkEvent));
    assertTrue(manager.isInKeywordMode);
    assertEquals('?', manager.activeKeyword);
    assertEquals('', manager.inputKeywordModel?.displayText);
  });

  test('handleBackspace not in keyword mode', () => {
    const inputState = {
      value: 'query',
      selectionStart: 0,
      selectionEnd: 0,
    };
    assertFalse(manager.handleBackspace(inputState));
  });

  test('handleBackspace cursor not at 0', () => {
    manager.enter('google.com', 'Search Google', KeywordModeEntryMethod.TAB);
    assertFalse(manager.handleBackspace({
      value: 'query',
      selectionStart: 2,
      selectionEnd: 2,
    }));
  });

  test('handleBackspace non-collapsed selection at 0', () => {
    manager.enter('google.com', 'Search Google', KeywordModeEntryMethod.TAB);
    assertFalse(manager.handleBackspace({
      value: 'query',
      selectionStart: 0,
      selectionEnd: 3,
    }));
    assertTrue(manager.isInKeywordMode);
  });

  test(
      'handleBackspace tab entry without typing restores keyword without ' +
          'trailing space',
      () => {
        // Case 1: 'yout<tab><backspace>' -> restore 'youtube.com'
        const match = createSearchMatchForTesting({
          allowedToBeDefaultMatch: true,
          keywordModel: createMatchKeywordModelForTesting({
            type: KeywordType.kChip,
            keyword: 'youtube.com',
            chipHint: 'Search YouTube',
          }),
        });
        assertTrue(manager.acceptTab(match, /*matchIndex=*/ 0));

        assertTrue(manager.handleBackspace({
          value: '',
          selectionStart: 0,
          selectionEnd: 0,
        }));
        assertFalse(manager.isInKeywordMode);
        assertEquals('youtube.com', lastKeywordCleared?.restoredText);
        assertEquals(11, lastKeywordCleared?.cursorPosition);
      });

  test(
      'handleBackspace space entry restores keyword with trailing space',
      () => {
        // Case 2: 'youtube.com<space><backspace>' -> restore 'youtube.com '
        manager.inputKeywordModel = {
          type: KeywordType.kChip,
          keyword: 'youtube.com',
          displayText: 'Search YouTube',
        };
        assertTrue(manager.acceptInputTrigger('youtube.com ', 12, spaceEvent));

        assertTrue(manager.handleBackspace({
          value: '',
          selectionStart: 0,
          selectionEnd: 0,
        }));
        assertFalse(manager.isInKeywordMode);
        assertEquals('youtube.com ', lastKeywordCleared?.restoredText);
        assertEquals(12, lastKeywordCleared?.cursorPosition);
      });

  test(
      'handleBackspace space in middle entry restores keyword with space',
      () => {
        // 'youtube.comquery' -> space at 12 -> 'youtube.com query' -> backspace
        // at 0 restores 'youtube.com query'
        manager.availableKeywordModels = [{
          type: KeywordType.kChip,
          keyword: 'youtube.com',
          displayText: 'Search YouTube',
        }];
        assertFalse(
            manager.acceptInputTrigger('youtube.comquery', 16, spaceEvent));
        assertTrue(
            manager.acceptInputTrigger('youtube.com query', 12, spaceEvent));

        assertTrue(manager.handleBackspace({
          value: 'query',
          selectionStart: 0,
          selectionEnd: 0,
        }));
        assertFalse(manager.isInKeywordMode);
        assertEquals('youtube.com query', lastKeywordCleared?.restoredText);
        assertEquals(12, lastKeywordCleared?.cursorPosition);
      });

  test(
      'handleBackspace tab entry after typing restores keyword with space',
      () => {
        // Case 3: 'you<tab>q<left arrow><backspace>' -> restore 'youtube.com q'
        manager.enter(
            'youtube.com', 'Search YouTube', KeywordModeEntryMethod.TAB);

        assertTrue(manager.handleBackspace({
          value: 'q',
          selectionStart: 0,
          selectionEnd: 0,
        }));
        assertFalse(manager.isInKeywordMode);
        assertEquals('youtube.com q', lastKeywordCleared?.restoredText);
        assertEquals(12, lastKeywordCleared?.cursorPosition);
      });

  test(
      'handleBackspace question mark entry restores question mark prefix',
      () => {
        // Case 4: '?f<left arrow><backspace>' -> restore '?f'
        assertTrue(manager.acceptInputTrigger('?', 1, questionMarkEvent));

        assertTrue(manager.handleBackspace({
          value: 'f',
          selectionStart: 0,
          selectionEnd: 0,
        }));
        assertFalse(manager.isInKeywordMode);
        assertEquals('?f', lastKeywordCleared?.restoredText);
        assertEquals(1, lastKeywordCleared?.cursorPosition);

        // Immediate backspace: '?<backspace>' -> restore '?'
        assertTrue(manager.acceptInputTrigger('?', 1, questionMarkEvent));
        assertTrue(manager.handleBackspace({
          value: '',
          selectionStart: 0,
          selectionEnd: 0,
        }));
        assertFalse(manager.isInKeywordMode);
        assertEquals('?', lastKeywordCleared?.restoredText);
        assertEquals(1, lastKeywordCleared?.cursorPosition);
      });

  test(
      'handleBackspace keyboard shortcut entry does not restore keyword',
      () => {
        // Case 5: '<ctrl+K><backspace>' -> restore ''
        manager.enter(
            'google.com', 'Search Google',
            KeywordModeEntryMethod.KEYBOARD_SHORTCUT);
        assertTrue(manager.handleBackspace({
          value: '',
          selectionStart: 0,
          selectionEnd: 0,
        }));
        assertFalse(manager.isInKeywordMode);
        assertEquals('', lastKeywordCleared?.restoredText);
        assertEquals(0, lastKeywordCleared?.cursorPosition);

        // After typing: '<ctrl+K>abc<left arrow * 3><backspace>' -> restore
        // 'abc'
        manager.enter(
            'google.com', 'Search Google',
            KeywordModeEntryMethod.KEYBOARD_SHORTCUT);
        assertTrue(manager.handleBackspace({
          value: 'abc',
          selectionStart: 0,
          selectionEnd: 0,
        }));
        assertFalse(manager.isInKeywordMode);
        assertEquals('abc', lastKeywordCleared?.restoredText);
        assertEquals(0, lastKeywordCleared?.cursorPosition);
      });

  test('handleBackspace click entry restores keyword with space', () => {
    const match = createSearchMatchForTesting({
      keywordModel: createMatchKeywordModelForTesting({
        type: KeywordType.kChip,
        keyword: 'youtube.com',
        chipHint: 'Search YouTube',
      }),
    });
    manager.handleKeywordClick(match);

    // Immediate backspace restores keyword with space.
    assertTrue(manager.handleBackspace({
      value: '',
      selectionStart: 0,
      selectionEnd: 0,
    }));
    assertFalse(manager.isInKeywordMode);
    assertEquals('youtube.com ', lastKeywordCleared?.restoredText);
    assertEquals(12, lastKeywordCleared?.cursorPosition);

    // After typing restores keyword with space + typed text.
    manager.handleKeywordClick(match);
    assertTrue(manager.handleBackspace({
      value: 'query',
      selectionStart: 0,
      selectionEnd: 0,
    }));
    assertFalse(manager.isInKeywordMode);
    assertEquals('youtube.com query', lastKeywordCleared?.restoredText);
    assertEquals(12, lastKeywordCleared?.cursorPosition);
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
    manager.enter('google.com', 'Search Google', KeywordModeEntryMethod.TAB);
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

    manager.exit();

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
    manager.enter('youtube.com', 'Search YouTube', KeywordModeEntryMethod.TAB);
    manager.onSelectedMatchChanged(null);
    assertTrue(manager.inputKeywordModel !== null);
    assertEquals(KeywordType.kInKeyword, manager.inputKeywordModel?.type);
    assertEquals('youtube.com', manager.inputKeywordModel?.keyword);

    // Instant keyword match -> enters keyword mode immediately.
    const instantMatchBookmarks = createSearchMatchForTesting({
      keywordModel: createMatchKeywordModelForTesting({
        type: KeywordType.kInstant,
        keyword: '@bookmarks',
        chipHint: 'Bookmarks',
      }),
    });
    manager.onSelectedMatchChanged(instantMatchBookmarks);
    assertTrue(manager.isInKeywordMode);
    assertEquals(KeywordType.kInKeyword, manager.inputKeywordModel?.type);
    assertEquals('@bookmarks', manager.inputKeywordModel?.keyword);
    assertEquals('Bookmarks', manager.inputKeywordModel?.displayText);

    // Selecting another instant keyword match -> updates keyword mode.
    const instantMatchHistory = createSearchMatchForTesting({
      keywordModel: createMatchKeywordModelForTesting({
        type: KeywordType.kInstant,
        keyword: '@history',
        chipHint: 'History',
      }),
    });
    manager.onSelectedMatchChanged(instantMatchHistory);
    assertTrue(manager.isInKeywordMode);
    assertEquals('@history', manager.inputKeywordModel?.keyword);
    assertEquals('History', manager.inputKeywordModel?.displayText);

    // Navigating away to a match without keyword model -> exits keyword mode.
    manager.onSelectedMatchChanged(matchWithoutKeyword);
    assertFalse(manager.isInKeywordMode);
    assertEquals(null, manager.inputKeywordModel);

    // Match with keyword chip when chip is selected -> enters keyword mode.
    manager.onSelectedMatchChanged(
        matchWithKeyword,
        {line: 0, state: SelectionLineState.kKeywordMode, actionIndex: 0});
    assertTrue(manager.isInKeywordMode);
    assertEquals(KeywordType.kInKeyword, manager.inputKeywordModel?.type);
    assertEquals('youtube.com', manager.inputKeywordModel?.keyword);
    assertEquals('Search YouTube', manager.inputKeywordModel?.displayText);

    // Match with keyword chip differing only in case when already in keyword
    // mode
    // -> does not re-enter.
    manager.onSelectedMatchChanged(
        matchWithKeyword,
        {line: 0, state: SelectionLineState.kKeywordMode, actionIndex: 0});
    assertTrue(manager.isInKeywordMode);
    let enterCalls = 0;
    const originalEnter = manager.enter.bind(manager);
    manager.enter = (...args) => {
      enterCalls++;
      originalEnter(...args);
    };
    manager.onSelectedMatchChanged(
        createSearchMatchForTesting({
          keywordModel: createMatchKeywordModelForTesting({
            type: KeywordType.kChip,
            keyword: 'YOUTUBE.COM',
          }),
        }),
        {line: 0, state: SelectionLineState.kKeywordMode, actionIndex: 0});
    assertEquals(0, enterCalls);
    assertTrue(manager.isInKeywordMode);
    manager.enter = originalEnter;

    // Navigating away from keyword chip to action button -> exits keyword mode.
    manager.onSelectedMatchChanged(matchWithKeyword, {
      line: 0,
      state: SelectionLineState.kFocusedButtonAction,
      actionIndex: 0,
    });
    assertFalse(manager.isInKeywordMode);
    assertEquals(KeywordType.kChip, manager.inputKeywordModel?.type);
  });

  test('formatMatchFillIntoEdit in keyword mode', () => {
    manager.enter('youtube.com', 'Search YouTube', KeywordModeEntryMethod.TAB);

    // Exact keyword fill on default match with lastQueriedInput -> returns ''.
    const defaultKeywordMatch = createSearchMatchForTesting({
      fillIntoEdit: 'youtube.com',
      allowedToBeDefaultMatch: true,
      keywordModel: createMatchKeywordModelForTesting({
        type: KeywordType.kChip,
        keyword: 'youtube.com',
      }),
    });
    assertEquals(
        '',
        manager.formatMatchFillIntoEdit(
            defaultKeywordMatch, /*matchIndex=*/ 0,
            /*lastQueriedInput=*/ 'youtube.com'));

    // Keyword match with query fill -> returns query part.
    const searchMatch = createSearchMatchForTesting({
      fillIntoEdit: 'youtube.com funny cats',
    });
    assertEquals(
        'funny cats',
        manager.formatMatchFillIntoEdit(searchMatch, /*matchIndex=*/ 1));

    // Keyword match with case-differing query fill -> returns query part.
    const upperSearchMatch = createSearchMatchForTesting({
      fillIntoEdit: 'YouTube.com funny cats',
    });
    assertEquals(
        'funny cats',
        manager.formatMatchFillIntoEdit(upperSearchMatch, /*matchIndex=*/ 1));

    // Keyword match with exact keyword fill -> returns ''.
    const exactMatch = createSearchMatchForTesting({
      fillIntoEdit: 'youtube.com',
    });
    assertEquals(
        '', manager.formatMatchFillIntoEdit(exactMatch, /*matchIndex=*/ 1));

    // Keyword match with case-differing exact keyword fill -> returns ''.
    const upperExactMatch = createSearchMatchForTesting({
      fillIntoEdit: 'YOUTUBE.COM',
    });
    assertEquals(
        '',
        manager.formatMatchFillIntoEdit(upperExactMatch, /*matchIndex=*/ 1));
  });
});

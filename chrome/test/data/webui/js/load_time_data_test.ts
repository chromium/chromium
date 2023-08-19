// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('LoadTimeDataTest', function() {
  setup(function() {
    loadTimeData.resetForTesting();
  });

  test('getStringPieces', function() {
    function assertSubstitutedPieces(
        expected: Array<{value: string, arg: (null | string)}>, label: string,
        ...args: Array<string|number>) {
      const pieces = loadTimeData.getSubstitutedStringPieces(label, ...args);
      assertDeepEquals(expected, pieces);

      // Ensure output matches getStringF.
      assertEquals(
          loadTimeData.substituteString(label, ...args),
          pieces.map(p => p.value).join(''));
    }

    assertSubstitutedPieces([{value: 'paper', arg: null}], 'paper');
    assertSubstitutedPieces([{value: 'paper', arg: '$1'}], '$1', 'paper');

    assertSubstitutedPieces(
        [
          {value: 'i think ', arg: null},
          {value: 'paper mario', arg: '$1'},
          {value: ' is a good game', arg: null},
        ],
        'i think $1 is a good game', 'paper mario');

    assertSubstitutedPieces(
        [
          {value: 'paper mario', arg: '$1'},
          {value: ' costs $', arg: null},
          {value: '60', arg: '$2'},
        ],
        '$1 costs $$$2', 'paper mario', '60');

    assertSubstitutedPieces(
        [
          {value: 'paper mario', arg: '$1'},
          {value: ' costs $60', arg: null},
        ],
        '$1 costs $$60', 'paper mario');

    assertSubstitutedPieces(
        [
          {value: 'paper mario', arg: '$1'},
          {value: ' costs\n$60 ', arg: null},
          {value: 'today', arg: '$2'},
        ],
        '$1 costs\n$$60 $2', 'paper mario', 'today');

    assertSubstitutedPieces(
        [
          {value: '$$', arg: null},
          {value: '1', arg: '$1'},
          {value: '2', arg: '$2'},
          {value: '1', arg: '$1'},
          {value: '$$2', arg: null},
          {value: '2', arg: '$2'},
          {value: '$', arg: null},
          {value: '1', arg: '$1'},
          {value: '$', arg: null},
        ],
        '$$$$$1$2$1$$$$2$2$$$1$$', '1', '2');
  });

  test('unescapedDollarSign', function() {
    const error = 'Assertion failed: Unescaped $ found in localized string.';

    function assertSubstitutionThrows(label: string, ...args: string[]) {
      assertThrows(() => {
        loadTimeData.getSubstitutedStringPieces(label, ...args);
      }, error);

      assertThrows(() => {
        loadTimeData.substituteString(label, ...args);
      }, error);
    }

    assertSubstitutionThrows('$');
    assertSubstitutionThrows('$1$$$a2', 'foo');
    assertSubstitutionThrows('$$$');
    assertSubstitutionThrows('a$');
    assertSubstitutionThrows('a$\n');
  });

  test('isInitialized_and_resetForTesting', function() {
    // Should start as un-initialized.
    assertFalse(loadTimeData.isInitialized());

    // Setting the data should change the state to initialized.
    loadTimeData.data = {TEST_KEY: 'test value'};
    assertTrue(loadTimeData.isInitialized());

    // resetForTesting() should restore the un-initialized state.
    loadTimeData.resetForTesting();
    assertFalse(loadTimeData.isInitialized());

    // resetForTesting() to empty state which is considered initialized.
    loadTimeData.resetForTesting({});
    assertTrue(loadTimeData.isInitialized());

    // resetForTesting() to a specific state which is considered initialized.
    loadTimeData.resetForTesting({SOMETHING: 'ANYTHING'});
    assertTrue(loadTimeData.isInitialized());
    assertTrue(loadTimeData.valueExists('SOMETHING'));
  });
});

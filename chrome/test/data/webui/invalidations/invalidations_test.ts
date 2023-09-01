// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util_ts.js';

import {assertEquals, assertNotEquals, assertTrue} from '../chai_assert.js';

suite('InvalidationsTest', function() {
  // Test that registering an invalidations appears properly on the textarea.
  test('RegisterNewInvalidation', function() {
    const invalidationsLog =
        getRequiredElement<HTMLTextAreaElement>('invalidations-log');
    const invalidation = [
      {isUnknownVersion: 'true', objectId: {name: 'EXTENSIONS', source: 1004}},
    ];
    invalidationsLog.value = '';
    webUIListenerCallback('log-invalidations', invalidation);
    const isContained =
        invalidationsLog.value.indexOf(
            'Received Invalidation with type ' +
            '"EXTENSIONS" version "Unknown" with payload "undefined"') !== -1;
    assertTrue(isContained, 'Actual log is:' + invalidationsLog.value);
  });

  // Test that changing the Invalidations Service state appears both in the
  // span and in the textarea.
  test('ChangeInvalidationsState', function() {
    const invalidationsState =
        getRequiredElement<HTMLElement>('invalidations-state');
    const invalidationsLog =
        getRequiredElement<HTMLTextAreaElement>('invalidations-log');
    const newState = 'INVALIDATIONS_ENABLED';
    const newNewState = 'TRANSIENT_INVALIDATION_ERROR';

    webUIListenerCallback('state-updated', newState);
    const isContainedState =
        invalidationsState.textContent!.indexOf('INVALIDATIONS_ENABLED') !== -1;
    assertTrue(isContainedState, 'could not change the invalidations text');

    invalidationsLog.value = '';
    webUIListenerCallback('state-updated', newNewState);
    const isContainedState2 = invalidationsState.textContent!.indexOf(
                                  'TRANSIENT_INVALIDATION_ERROR') !== -1;
    assertTrue(isContainedState2, 'could not change the invalidations text');
    const isContainedLog = invalidationsLog.value.indexOf(
                               'Invalidations service state changed to ' +
                               '"TRANSIENT_INVALIDATION_ERROR"') !== -1;
    assertTrue(isContainedLog, 'Actual log is:' + invalidationsLog.value);
  });

  // Test that objects ids appear on the table.
  test('RegisterNewIds', function() {
    interface DataType {
      name: string;
      source: number;
      totalCount?: number;
    }

    let newDataType: DataType[] = [
      {name: 'EXTENSIONS', source: 1004, totalCount: 0},
      {name: 'FAVICON_IMAGE', source: 1004, totalCount: 0},
    ];

    const registrarName = 'Fake';
    const pattern1 =
        [registrarName, '1004', 'EXTENSIONS', '0', '0', '', '', ''];
    const pattern2 =
        [registrarName, '1004', 'FAVICON_IMAGE', '0', '0', '', '', ''];
    // Register two objects ID with 'Fake' registrar
    webUIListenerCallback('ids-updated', registrarName, newDataType);
    // Disable the Extensions ObjectId by only sending FAVICON_IMAGE
    newDataType = [{name: 'FAVICON_IMAGE', source: 1004}];
    webUIListenerCallback('ids-updated', registrarName, newDataType);

    // Test that the two patterns are contained in the table.
    const oidTable =
        getRequiredElement<HTMLTableElement>('objectsid-table-container');
    let foundPattern1 = false;
    let foundPattern2 = false;
    for (const row of oidTable.rows) {
      let pattern1Test = true;
      let pattern2Test = true;
      for (let cell = 0; cell < row.cells.length; cell++) {
        pattern1Test =
            pattern1Test && (pattern1[cell] === row.cells[cell]!.textContent);
        pattern2Test =
            pattern2Test && (pattern2[cell] === row.cells[cell]!.textContent);
      }
      if (pattern1Test) {
        assertEquals('greyed', row.className);
      }
      if (pattern2Test) {
        assertEquals('content', row.className);
      }

      foundPattern1 = foundPattern1 || pattern1Test;
      foundPattern2 = foundPattern2 || pattern2Test;
      if (foundPattern2) {
        assertTrue(foundPattern1, 'The entries were not ordererd');
      }
    }
    assertTrue(
        foundPattern1 && foundPattern2, 'couldn\'t find both objects ids');
  });

  // Test that registering new handlers appear on the website.
  test('UpdateRegisteredHandlers', function() {
    function text() {
      return getRequiredElement<HTMLElement>('registered-handlers').textContent!
          ;
    }
    webUIListenerCallback('handlers-updated', ['FakeApi', 'FakeClient']);
    assertNotEquals(text().indexOf('FakeApi'), -1);
    assertNotEquals(text().indexOf('FakeClient'), -1);

    webUIListenerCallback('handlers-updated', ['FakeClient']);
    assertEquals(text().indexOf('FakeApi'), -1);
    assertNotEquals(text().indexOf('FakeClient'), -1);
  });

  // Test that an object showing internal state is correctly displayed.
  test('UpdateInternalDisplay', function() {
    const newDetailedStatus = {MessagesSent: 1};
    webUIListenerCallback('detailed-status-updated', newDetailedStatus);
    assertEquals(
        getRequiredElement<HTMLTextAreaElement>('internal-display').value,
        '{\n  \"MessagesSent\": 1\n}');
  });
});

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';
import {assertEquals, assertNotEquals, assertTrue} from '../chai_assert.js';

window.invalidations_test = {};
invalidations_test = window.invalidations_test;
invalidations_test.TestNames = {
  RegisterNewInvalidation: 'register new invalidation',
  ChangeInvalidationsState: 'change invalidations state',
  RegisterNewIds: 'register new ids',
  UpdateRegisteredHandlers: 'update registered handlers',
  UpdateInternalDisplay: 'update internal display',
};

suite('invalidations_test', function() {
  // Test that registering an invalidations appears properly on the textarea.
  test(invalidations_test.TestNames.RegisterNewInvalidation, function() {
    var invalidationsLog = $('invalidations-log');
    var invalidation = [
      {isUnknownVersion: 'true', objectId: {name: 'EXTENSIONS', source: 1004}}
    ];
    invalidationsLog.value = '';
    webUIListenerCallback('log-invalidations', invalidation);
    var isContained =
        invalidationsLog.value.indexOf(
            'Received Invalidation with type ' +
            '"EXTENSIONS" version "Unknown" with payload "undefined"') !== -1;
    assertTrue(isContained, 'Actual log is:' + invalidationsLog.value);
  });

  // Test that changing the Invalidations Service state appears both in the
  // span and in the textarea.
  test(invalidations_test.TestNames.ChangeInvalidationsState, function() {
    var invalidationsState = $('invalidations-state');
    var invalidationsLog = $('invalidations-log');
    var newState = 'INVALIDATIONS_ENABLED';
    var newNewState = 'TRANSIENT_INVALIDATION_ERROR';

    webUIListenerCallback('state-updated', newState);
    var isContainedState =
        invalidationsState.textContent.indexOf('INVALIDATIONS_ENABLED') !== -1;
    assertTrue(isContainedState, 'could not change the invalidations text');

    invalidationsLog.value = '';
    webUIListenerCallback('state-updated', newNewState);
    var isContainedState2 = invalidationsState.textContent.indexOf(
                                'TRANSIENT_INVALIDATION_ERROR') !== -1;
    assertTrue(isContainedState2, 'could not change the invalidations text');
    var isContainedLog = invalidationsLog.value.indexOf(
                             'Invalidations service state changed to ' +
                             '"TRANSIENT_INVALIDATION_ERROR"') !== -1;
    assertTrue(isContainedLog, 'Actual log is:' + invalidationsLog.value);
  });

  // Test that objects ids appear on the table.
  test(invalidations_test.TestNames.RegisterNewIds, function() {
    var newDataType = [
      {name: 'EXTENSIONS', source: 1004, totalCount: 0},
      {name: 'FAVICON_IMAGE', source: 1004, totalCount: 0}
    ];
    const registrarName = 'Fake';
    var pattern1 = [registrarName, '1004', 'EXTENSIONS', '0', '0', '', '', ''];
    var pattern2 =
        [registrarName, '1004', 'FAVICON_IMAGE', '0', '0', '', '', ''];
    // Register two objects ID with 'Fake' registrar
    webUIListenerCallback('ids-updated', registrarName, newDataType);
    // Disable the Extensions ObjectId by only sending FAVICON_IMAGE
    newDataType = [{name: 'FAVICON_IMAGE', source: 1004}];
    webUIListenerCallback('ids-updated', registrarName, newDataType);

    // Test that the two patterns are contained in the table.
    var oidTable = $('objectsid-table-container');
    var foundPattern1 = false;
    var foundPattern2 = false;
    for (var row = 0; row < oidTable.rows.length; row++) {
      var pattern1Test = true;
      var pattern2Test = true;
      for (var cell = 0; cell < oidTable.rows[row].cells.length; cell++) {
        pattern1Test = pattern1Test &&
            (pattern1[cell] === oidTable.rows[row].cells[cell].textContent);
        pattern2Test = pattern2Test &&
            (pattern2[cell] === oidTable.rows[row].cells[cell].textContent);
      }
      if (pattern1Test) {
        assertEquals('greyed', oidTable.rows[row].className);
      }
      if (pattern2Test) {
        assertEquals('content', oidTable.rows[row].className);
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
  test(invalidations_test.TestNames.UpdateRegisteredHandlers, function() {
    function text() {
      return $('registered-handlers').textContent;
    }
    webUIListenerCallback('handlers-updated', ['FakeApi', 'FakeClient']);
    assertNotEquals(text().indexOf('FakeApi'), -1);
    assertNotEquals(text().indexOf('FakeClient'), -1);

    webUIListenerCallback('handlers-updated', ['FakeClient']);
    assertEquals(text().indexOf('FakeApi'), -1);
    assertNotEquals(text().indexOf('FakeClient'), -1);
  });

  // Test that an object showing internal state is correctly displayed.
  test(invalidations_test.TestNames.UpdateInternalDisplay, function() {
    const newDetailedStatus = {MessagesSent: 1};
    webUIListenerCallback('detailed-status-updated', newDetailedStatus);
    assertEquals($('internal-display').value, '{\n  \"MessagesSent\": 1\n}');
  });
});

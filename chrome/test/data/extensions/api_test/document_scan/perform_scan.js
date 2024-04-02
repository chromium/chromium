// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These tests should be run with get scanner list permission allowed and start
// scan permission allowed (or the extension should be trusted).
chrome.test.runTests([
  async function getScannerListTwoCalls() {
    const filter = {
      local: true,
      secure: true,
    };
    var savedId;
    chrome.documentScan.getScannerList(filter, response => {
      chrome.test.assertEq(OperationResult.SUCCESS, response.result);
      chrome.test.assertEq(1, response.scanners.length);
      chrome.test.assertEq('scanneridabc123', response.scanners[0].scannerId);
      chrome.test.assertEq('GoogleTest', response.scanners[0].manufacturer);
      chrome.test.assertEq('Scanner', response.scanners[0].model);
      chrome.test.assertEq('Mopria', response.scanners[0].protocolType);

      // Second call should get the same scanner with the same id because it's
      // the same extension within the same session.
      chrome.documentScan.getScannerList(filter, response => {
        chrome.test.assertEq(OperationResult.SUCCESS, response.result);
        chrome.test.assertEq(1, response.scanners.length);
        chrome.test.assertEq('scanneridabc123', response.scanners[0].scannerId);
        chrome.test.assertEq('GoogleTest', response.scanners[0].manufacturer);
        chrome.test.assertEq('Scanner', response.scanners[0].model);
        chrome.test.assertEq('Mopria', response.scanners[0].protocolType);
        chrome.test.succeed();
      });
    });
  },

  async function openBeforeListFails() {
    let response = await openScanner('scanner');
    chrome.test.assertEq('scanner', response.scannerId);
    chrome.test.assertEq(OperationResult.INVALID, response.result);
    chrome.test.assertEq(null, response.scannerHandle);
    chrome.test.assertEq(null, response.scannerOptions);
    chrome.test.succeed();
  },

  async function getOptionGroupsInvalidHandleFails() {
    const response = await getOptionGroups('invalid-handle');
    chrome.test.assertEq(chrome.documentScan.OperationResult.INVALID,
                         response.result);
    chrome.test.assertEq('invalid-handle', response.scannerHandle);
    chrome.test.assertEq(null, response.groups);
    chrome.test.succeed();
  },

  async function getOptionGroupsSuccess() {
    const scannerHandle = await getScannerHandle();
    chrome.test.assertNe(null, scannerHandle);
    const response = await getOptionGroups(scannerHandle);
    chrome.test.assertEq(chrome.documentScan.OperationResult.SUCCESS,
                         response.result);
    chrome.test.assertEq(scannerHandle, response.scannerHandle);
    chrome.test.assertNe(null, response.groups);
    chrome.test.assertEq(1, response.groups.length);
    chrome.test.assertEq('title', response.groups[0].title);
    chrome.test.assertEq(2, response.groups[0].members.length);
    chrome.test.assertEq('item1', response.groups[0].members[0]);
    chrome.test.assertEq('item2', response.groups[0].members[1]);
    chrome.test.succeed();
  },

  async function closeBeforeOpenFails() {
    let response = await closeScanner('scanner');
    chrome.test.assertEq('scanner', response.scannerHandle);
    chrome.test.assertEq(OperationResult.INVALID, response.result);
    chrome.test.succeed();
  },

  async function closeOpenHandleSucceeds() {
    let scannerId = await getScannerId();
    chrome.test.assertNe(null, scannerId);

    let openResponse = await openScanner(scannerId);
    chrome.test.assertEq(scannerId, openResponse.scannerId);
    chrome.test.assertEq(OperationResult.SUCCESS, openResponse.result);
    chrome.test.assertNe(null, openResponse.scannerHandle);
    const scannerHandle = openResponse.scannerHandle;

    let closeResponse = await closeScanner(scannerHandle);
    chrome.test.assertEq(scannerHandle, closeResponse.scannerHandle);
    chrome.test.assertEq(OperationResult.SUCCESS, closeResponse.result);

    // Closing the same handle a second time fails.
    closeResponse = await closeScanner(scannerHandle);
    chrome.test.assertEq(scannerHandle, closeResponse.scannerHandle);
    chrome.test.assertEq(OperationResult.INVALID, closeResponse.result);

    chrome.test.succeed();
  },

  async function reopenHandleSucceeds() {
    let scannerId = await getScannerId();
    chrome.test.assertNe(null, scannerId);

    let openResponse = await openScanner(scannerId);
    chrome.test.assertEq(scannerId, openResponse.scannerId);
    chrome.test.assertEq(OperationResult.SUCCESS, openResponse.result);
    chrome.test.assertNe(null, openResponse.scannerHandle);

    // Reopening the same scannerId succeeds.  Check for a non-empty handle, but
    // don't check if it's different from the previous handle because the
    // backend doesn't guarantee to send a different value each time.
    openResponse = await openScanner(scannerId);
    chrome.test.assertEq(scannerId, openResponse.scannerId);
    chrome.test.assertEq(OperationResult.SUCCESS, openResponse.result);
    chrome.test.assertNe(null, openResponse.scannerHandle);

    // Get a new scannerId pointing to the same scanner.
    scannerId = await getScannerId();
    chrome.test.assertNe(null, scannerId);

    // Reopening the same scanner via a new scannerId succeeds.
    openResponse = await openScanner(scannerId);
    chrome.test.assertEq(scannerId, openResponse.scannerId);
    chrome.test.assertEq(OperationResult.SUCCESS, openResponse.result);
    chrome.test.assertNe(null, openResponse.scannerHandle);

    chrome.test.succeed();
  },

  async function startAndCancelScan() {
    const scannerHandle = await getScannerHandle();
    chrome.test.assertNe(null, scannerHandle);

    const startResponse = await startScan(scannerHandle);
    const jobHandle = startResponse.job;
    chrome.test.assertEq(scannerHandle, startResponse.scannerHandle);
    chrome.test.assertEq(OperationResult.SUCCESS, startResponse.result);
    chrome.test.assertNe(null, jobHandle);

    const cancelResponse = await cancelScan(jobHandle);
    chrome.test.assertEq(jobHandle, cancelResponse.job);
    chrome.test.assertEq(OperationResult.SUCCESS, cancelResponse.result);
    chrome.test.succeed();
  },

  async function startScanZeroMaxSizeFails() {
    const scannerHandle = await getScannerHandle();
    chrome.test.assertNe(null, scannerHandle);

    const startResponse = await startScan(scannerHandle, 0);
    chrome.test.assertEq(scannerHandle, startResponse.scannerHandle);
    chrome.test.assertEq(OperationResult.INVALID, startResponse.result);
    chrome.test.assertEq(null, startResponse.job);
    chrome.test.succeed();
  },

  async function startScanInvalidMaxSizeFails() {
    const scannerHandle = await getScannerHandle();
    chrome.test.assertNe(null, scannerHandle);

    const startResponse = await startScan(scannerHandle, 32767);
    chrome.test.assertEq(scannerHandle, startResponse.scannerHandle);
    chrome.test.assertEq(OperationResult.INVALID, startResponse.result);
    chrome.test.assertEq(null, startResponse.job);
    chrome.test.succeed();
  },

  async function starScanValidMaxSizeSucceeds() {
    const scannerHandle = await getScannerHandle();
    chrome.test.assertNe(null, scannerHandle);

    const startResponse = await startScan(scannerHandle, 32768);
    const jobHandle = startResponse.job;
    chrome.test.assertEq(scannerHandle, startResponse.scannerHandle);
    chrome.test.assertEq(OperationResult.SUCCESS, startResponse.result);
    chrome.test.assertNe(null, jobHandle);

    const cancelResponse = await cancelScan(jobHandle);
    chrome.test.assertEq(jobHandle, cancelResponse.job);
    chrome.test.assertEq(OperationResult.SUCCESS, cancelResponse.result);
    chrome.test.succeed();
  },

  async function startScanInvalidHandleFails() {
    const startResponse = await startScan('invalid-handle');
    chrome.test.assertEq('invalid-handle', startResponse.scannerHandle);
    chrome.test.assertEq(OperationResult.INVALID, startResponse.result);
    chrome.test.assertEq(null, startResponse.job);
    chrome.test.succeed();
  },

  async function cancelScanInvalidHandleFails() {
    const cancelResponse = await cancelScan('invalid-handle');
    chrome.test.assertEq('invalid-handle', cancelResponse.job);
    chrome.test.assertEq(OperationResult.INVALID, cancelResponse.result);
    chrome.test.succeed();
  },

  async function getListClearsHandles() {
    const scannerHandle = await getScannerHandle();
    chrome.test.assertNe(null, scannerHandle);

    const startResponse = await startScan(scannerHandle);
    const jobHandle = startResponse.job;
    chrome.test.assertEq(scannerHandle, startResponse.scannerHandle);
    chrome.test.assertEq(OperationResult.SUCCESS, startResponse.result);
    chrome.test.assertNe(null, jobHandle);

    // If a user calls getScannerList, open handles will be closed and running
    // jobs will be canceled.  They are all invalid after it returns.
    const filter = {
      local: true,
      secure: true,
    };
    let getListResponse = await getScannerList(filter);
    chrome.test.assertEq(OperationResult.SUCCESS, getListResponse.result);

    const cancelResponse = await cancelScan(jobHandle);
    chrome.test.assertEq(jobHandle, cancelResponse.job);
    chrome.test.assertEq(OperationResult.INVALID, cancelResponse.result);

    const startResponse2 = await startScan(scannerHandle);
    chrome.test.assertEq(scannerHandle, startResponse2.scannerHandle);
    chrome.test.assertEq(OperationResult.INVALID, startResponse2.result);
    chrome.test.assertEq(null, startResponse2.job);

    chrome.test.succeed();
  },

  async function readScanDataBeforeStartFails() {
    const response = await readScanData('invalid-job');
    chrome.test.assertEq('invalid-job', response.job);
    chrome.test.assertEq(OperationResult.INVALID, response.result);
    chrome.test.assertEq(null, response.data);
    chrome.test.assertEq(null, response.estimatedCompletion);
    chrome.test.succeed();
  },

  async function readScanDataOnOpenHandleSucceeds() {
    const scannerHandle = await getScannerHandle();
    chrome.test.assertNe(null, scannerHandle);

    const startResponse = await startScan(scannerHandle);
    chrome.test.assertNe(null, startResponse.job);
    const jobHandle = startResponse.job;

    // Read succeeds because the job is active and owned by this extension.
    const readResponse1 = await readScanData(jobHandle);
    chrome.test.assertEq(jobHandle, readResponse1.job);
    chrome.test.assertEq(OperationResult.SUCCESS, readResponse1.result);
    chrome.test.assertNe(null, readResponse1.data);
    chrome.test.assertNe(null, readResponse1.estimatedCompletion);

    // Second read succeeds because the job is still active.
    const readResponse2 = await readScanData(jobHandle);
    chrome.test.assertEq(jobHandle, readResponse2.job);
    chrome.test.assertEq(OperationResult.SUCCESS, readResponse2.result);
    chrome.test.assertNe(null, readResponse2.data);
    chrome.test.assertNe(null, readResponse2.estimatedCompletion);

    chrome.test.succeed();
  },

  async function readScanDataOnCancelledHandleFails() {
    const scannerHandle = await getScannerHandle();
    chrome.test.assertNe(null, scannerHandle);

    const startResponse = await startScan(scannerHandle);
    chrome.test.assertNe(null, startResponse.job);
    const jobHandle = startResponse.job;

    // Read succeeds because the job is active and owned by this extension.
    const readResponse1 = await readScanData(jobHandle);
    chrome.test.assertEq(jobHandle, readResponse1.job);
    chrome.test.assertEq(OperationResult.SUCCESS, readResponse1.result);
    chrome.test.assertNe(null, readResponse1.data);
    chrome.test.assertNe(null, readResponse1.estimatedCompletion);

    // Cancelling the job invalidates the handle.
    const cancelResponse = await cancelScan(jobHandle);
    chrome.test.assertEq(jobHandle, readResponse1.job);
    chrome.test.assertEq(OperationResult.SUCCESS, readResponse1.result);

    // Second read reports cancelled because the job handle is valid but no
    // longer active.
    const readResponse2 = await readScanData(jobHandle);
    chrome.test.assertEq(jobHandle, readResponse2.job);
    chrome.test.assertEq(OperationResult.CANCELLED, readResponse2.result);
    chrome.test.assertEq(null, readResponse2.data);
    chrome.test.assertEq(null, readResponse2.estimatedCompletion);

    chrome.test.succeed();
  },

  async function setOptionsBeforeOpenFails() {
    const response = await setOptions('invalid-handle', [
        {name: 'option', type: OptionType.INT}]);
    chrome.test.assertEq('invalid-handle', response.scannerHandle);
    chrome.test.assertEq(1, response.results.length);
    chrome.test.assertEq(OperationResult.INVALID, response.results[0].result);
    chrome.test.assertEq('option', response.results[0].name);
    chrome.test.assertEq(null, response.options);
    chrome.test.succeed();
  },

  async function setOptionsRequiresMatchingTypes_Fixed() {
    // Fixed options can be set from int or double because JS doesn't have a
    // clear distinction between these.
    const options = [
      {name: 'fixed1', type: OptionType.FIXED, value: 42},        // OK, mapped.
      {name: 'fixed2', type: OptionType.FIXED, value: 42.0},      // OK, mapped.
      {name: 'fixed3', type: OptionType.FIXED, value: 42.5},      // OK.
      {name: 'fixed4', type: OptionType.FIXED, value: '1.0'},     // Wrong type.
      {name: 'fixed5', type: OptionType.FIXED, value: [42, 43]},  // OK, mapped.
      {name: 'fixed6', type: OptionType.FIXED,
          value: [42.0, 43.0]},                                   // OK, mapped.
      {name: 'fixed7', type: OptionType.FIXED, value: [42.5, 43.5]}  // OK.
    ];

    const scannerHandle = await getScannerHandle();
    chrome.test.assertNe(null, scannerHandle);

    const response = await setOptions(scannerHandle, options);
    chrome.test.assertEq(scannerHandle, response.scannerHandle);
    chrome.test.assertEq(options.length, response.results.length);
    // Match each result individually instead of one big array to make it easier
    // to tell where any failures occur.
    chrome.test.assertEq(
      {name: 'fixed1', result: OperationResult.SUCCESS}, response.results[0]);
    chrome.test.assertEq(
      {name: 'fixed2', result: OperationResult.SUCCESS}, response.results[1]);
    chrome.test.assertEq(
      {name: 'fixed3', result: OperationResult.SUCCESS}, response.results[2]);
    chrome.test.assertEq(
      {name: 'fixed4', result: OperationResult.WRONG_TYPE},
      response.results[3]);
    chrome.test.assertEq(
      {name: 'fixed5', result: OperationResult.SUCCESS}, response.results[4]);
    chrome.test.assertEq(
      {name: 'fixed6', result: OperationResult.SUCCESS}, response.results[5]);
    chrome.test.assertEq(
      {name: 'fixed7', result: OperationResult.SUCCESS}, response.results[6]);

    chrome.test.assertNe(null, response.options);
    chrome.test.succeed();
  },

  async function setOptionsRequiresMatchingTypes_Int() {
    // Int options can be set from int values or from double values with a zero
    // fractional part because JS doesn't have a clear distinction between
    // these.
    const options = [
      {name: 'int1', type: OptionType.INT, value: 42},            // OK.
      {name: 'int2', type: OptionType.INT, value: 42.0},          // OK, mapped.
      {name: 'int3', type: OptionType.INT, value: 42.5},          // Wrong type.
      {name: 'int4', type: OptionType.INT, value: '1.0'},         // Wrong type.
      {name: 'int5', type: OptionType.INT, value: [42, 42]},      // OK.
      {name: 'int6', type: OptionType.INT, value: [42.0, 42.0]},  // OK, mapped.
      {name: 'int7', type: OptionType.INT, value: [42.5]},        // Wrong type.
      {name: 'int8', type: OptionType.INT, value: 1e300},         // Wrong type.
      {name: 'int9', type: OptionType.INT, value: -1e300},        // Wrong type.
      {name: 'int10', type: OptionType.INT, value: [1e300]},      // Wrong type.
      {name: 'int11', type: OptionType.INT, value: [-1e300]}      // Wrong type.
    ];

    const scannerHandle = await getScannerHandle();
    chrome.test.assertNe(null, scannerHandle);

    const response = await setOptions(scannerHandle, options);
    chrome.test.assertEq(scannerHandle, response.scannerHandle);
    chrome.test.assertEq(options.length, response.results.length);
    // Match each result individually instead of one big array to make it easier
    // to tell where any failures occur.
    chrome.test.assertEq(
      {name: 'int1', result: OperationResult.SUCCESS}, response.results[0]);
    chrome.test.assertEq(
      {name: 'int2', result: OperationResult.SUCCESS}, response.results[1]);
    chrome.test.assertEq(
      {name: 'int3', result: OperationResult.WRONG_TYPE}, response.results[2]);
    chrome.test.assertEq(
      {name: 'int4', result: OperationResult.WRONG_TYPE}, response.results[3]);
    chrome.test.assertEq(
      {name: 'int5', result: OperationResult.SUCCESS}, response.results[4]);
    chrome.test.assertEq(
      {name: 'int6', result: OperationResult.SUCCESS}, response.results[5]);
    chrome.test.assertEq(
      {name: 'int7', result: OperationResult.WRONG_TYPE}, response.results[6]);
    chrome.test.assertEq(
      {name: 'int8', result: OperationResult.WRONG_TYPE}, response.results[7]);
    chrome.test.assertEq(
      {name: 'int9', result: OperationResult.WRONG_TYPE}, response.results[8]);
    chrome.test.assertEq(
      {name: 'int10', result: OperationResult.WRONG_TYPE},
      response.results[9]);
    chrome.test.assertEq(
      {name: 'int11', result: OperationResult.WRONG_TYPE},
      response.results[10]);

    chrome.test.assertNe(null, response.options);
    chrome.test.succeed();
  },

  async function setOptionsRequiresMatchingTypes_Bool() {
    // Bool options can only be set from a bool.
    const options = [
      {name: 'bool1', type: OptionType.BOOL, value: true},    // OK.
      {name: 'bool2', type: OptionType.BOOL, value: 1},       // Wrong type.
      {name: 'bool3', type: OptionType.BOOL, value: 'true'},  // Wrong type.
      {name: 'bool4', type: OptionType.BOOL, value: [1]}      // Wrong type.
    ];

    const scannerHandle = await getScannerHandle();
    chrome.test.assertNe(null, scannerHandle);

    const response = await setOptions(scannerHandle, options);
    chrome.test.assertEq(scannerHandle, response.scannerHandle);
    chrome.test.assertEq(options.length, response.results.length);
    // Match each result individually instead of one big array to make it easier
    // to tell where any failures occur.
    chrome.test.assertEq(
      {name: 'bool1', result: OperationResult.SUCCESS}, response.results[0]);
    chrome.test.assertEq(
      {name: 'bool2', result: OperationResult.WRONG_TYPE},
      response.results[1]);
    chrome.test.assertEq(
      {name: 'bool3', result: OperationResult.WRONG_TYPE},
      response.results[2]);
    chrome.test.assertEq(
      {name: 'bool4', result: OperationResult.WRONG_TYPE},
      response.results[3]);

    chrome.test.assertNe(null, response.options);
    chrome.test.succeed();
  },

  async function setOptionsRequiresMatchingTypes_String() {
    // String options can only be set from a string.
    const options = [
      {name: 'string1', type: OptionType.STRING, value: 's'},   // OK.
      {name: 'string2', type: OptionType.STRING, value: ''},    // OK.
      {name: 'string3', type: OptionType.STRING, value: 1},     // Wrong type.
      {name: 'string4', type: OptionType.STRING, value: [1]},   // Wrong type.
      {name: 'string5', type: OptionType.STRING, value: true},  // Wrong type.
    ];

    const scannerHandle = await getScannerHandle();
    chrome.test.assertNe(null, scannerHandle);

    const response = await setOptions(scannerHandle, options);
    chrome.test.assertEq(scannerHandle, response.scannerHandle);
    chrome.test.assertEq(options.length, response.results.length);
    // Match each result individually instead of one big array to make it easier
    // to tell where any failures occur.
    chrome.test.assertEq(
      {name: 'string1', result: OperationResult.SUCCESS}, response.results[0]);
    chrome.test.assertEq(
      {name: 'string2', result: OperationResult.SUCCESS}, response.results[1]);
    chrome.test.assertEq(
      {name: 'string3', result: OperationResult.WRONG_TYPE},
      response.results[2]);
    chrome.test.assertEq(
      {name: 'string4', result: OperationResult.WRONG_TYPE},
      response.results[3]);
    chrome.test.assertEq(
      {name: 'string5', result: OperationResult.WRONG_TYPE},
      response.results[4]);

    chrome.test.assertNe(null, response.options);
    chrome.test.succeed();
  }
]);

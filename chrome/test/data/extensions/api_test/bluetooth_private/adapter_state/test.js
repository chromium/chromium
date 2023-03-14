// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var newAdapterName = 'Dome';

function testSetAdapterState() {
  chrome.bluetooth.getAdapterState(function(state) {
    chrome.test.assertNoLastError();
    chrome.test.assertFalse(state.powered);
    chrome.test.assertNe(newAdapterName, state.name);
    // TODO(tengs): Check if adapter is discoverable when the attribute is
    // exposed to the chrome.bluetooth API.
    setAdapterState();
  });
}

function setAdapterState() {
  var newState = {
    name: newAdapterName,
    powered: true,
    discoverable: true
  };

  chrome.bluetoothPrivate.setAdapterState(newState, function() {
    chrome.test.assertNoLastError();
    if (chrome.runtime.lastError)
      chrome.test.fail(chrome.runtime.lastError);
    checkFinalAdapterState();
  });
}

var adapterStateSet = false;
function checkFinalAdapterState() {
  chrome.bluetooth.getAdapterState(function(state) {
    chrome.test.assertNoLastError();
    chrome.test.assertTrue(state.powered);
    chrome.test.assertTrue(state.name == newAdapterName);
    // TODO(tengs): Check if adapter is discoverable when the attribute is
    // exposed to the chrome.bluetooth API.
    if (!adapterStateSet) {
      adapterStateSet = true;
      // Check indempotence of bluetoothPrivate.setAdapterState.
      setAdapterState();
    } else {
      chrome.test.succeed();
    }
  });
}

chrome.test.runTests([ testSetAdapterState ]);

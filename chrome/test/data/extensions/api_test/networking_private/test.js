// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This just tests the interface. It does not test for specific results, only
// that callbacks are correctly invoked, expected parameters are correct,
// and failures are detected.

const callbackPass = chrome.test.callbackPass;

const FAILURE = 'Failure';
const GUID = 'SOME_GUID';

function callbackResult(result) {
  if (chrome.runtime.lastError) {
    chrome.test.fail(chrome.runtime.lastError.message);
  } else if (result == false || result == FAILURE) {
    chrome.test.fail(`Failed: ${result}`);
  }
}

const availableTests = [
  function getProperties() {
    chrome.networkingPrivate.getProperties(GUID, callbackPass(callbackResult));
  },
  function getManagedProperties() {
    chrome.networkingPrivate.getManagedProperties(
        GUID, callbackPass(callbackResult));
  },
  function getState() {
    chrome.networkingPrivate.getState(GUID, callbackPass(callbackResult));
  },
  function setProperties() {
    chrome.networkingPrivate.setProperties(
        GUID, {GUID: GUID}, callbackPass(callbackResult));
  },
  function createNetwork() {
    chrome.networkingPrivate.createNetwork(
        false, {GUID: GUID}, callbackPass(callbackResult));
  },
  function forgetNetwork() {
    chrome.networkingPrivate.forgetNetwork(GUID, callbackPass(callbackResult));
  },
  function getNetworks() {
    chrome.networkingPrivate.getNetworks(
        {networkType: 'Ethernet'}, callbackPass(callbackResult));
  },
  function getVisibleNetworks() {
    chrome.networkingPrivate.getVisibleNetworks(
        'Ethernet', callbackPass(callbackResult));
  },
  function getEnabledNetworkTypes() {
    chrome.networkingPrivate.getEnabledNetworkTypes(
        callbackPass(callbackResult));
  },
  function getDeviceStates() {
    chrome.networkingPrivate.getDeviceStates(callbackPass(callbackResult));
  },
  function enableNetworkType() {
    chrome.networkingPrivate.enableNetworkType('Ethernet');
    chrome.test.succeed();
  },
  function disableNetworkType() {
    chrome.networkingPrivate.disableNetworkType('Ethernet');
    chrome.test.succeed();
  },
  function requestNetworkScan() {
    chrome.networkingPrivate.requestNetworkScan();
    chrome.networkingPrivate.requestNetworkScan('Cellular');
    chrome.test.succeed();
  },
  function startConnect() {
    chrome.networkingPrivate.startConnect(GUID, callbackPass(callbackResult));
  },
  function startDisconnect() {
    chrome.networkingPrivate.startDisconnect(
        GUID, callbackPass(callbackResult));
  },
  function startActivate() {
    chrome.networkingPrivate.startActivate(
        GUID, '' /* carrier */, callbackPass(callbackResult));
  },
  function getCaptivePortalStatus() {
    chrome.networkingPrivate.getCaptivePortalStatus(
        GUID, callbackPass(callbackResult));
  },
  function unlockCellularSim() {
    chrome.networkingPrivate.unlockCellularSim(
        GUID, '1111', callbackPass(callbackResult));
  },
  function setCellularSimState() {
    const simState = {requirePin: true, currentPin: '1111', newPin: '1234'};
    chrome.networkingPrivate.setCellularSimState(
        GUID, simState, callbackPass(callbackResult));
  },
  function selectCellularMobileNetwork() {
    chrome.networkingPrivate.selectCellularMobileNetwork(
        GUID, 'fakeId', callbackPass(callbackResult));
  },
  function getGlobalPolicy() {
    chrome.networkingPrivate.getGlobalPolicy(callbackPass(callbackResult));
  },
];

const testToRun = window.location.search.substring(1);
chrome.test.runTests(availableTests.filter(function(op) {
  return op.name == testToRun;
}));

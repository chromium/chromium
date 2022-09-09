// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This just tests the interface. It does not test for specific results, only
// that callbacks are correctly invoked, expected parameters are correct,
// and failures are detected.

var callbackPass = chrome.test.callbackPass;

var kFailure = 'Failure';
var kGuid = 'SOME_GUID';

function callbackResult(result) {
  if (chrome.runtime.lastError)
    chrome.test.fail(chrome.runtime.lastError.message);
  else if (result == false || result == kFailure)
    chrome.test.fail('Failed: ' + result);
}

var availableTests = [
  function getProperties() {
    chrome.networkingPrivate.getProperties(
        kGuid, callbackPass(callbackResult));
  },
  function getManagedProperties() {
    chrome.networkingPrivate.getManagedProperties(
        kGuid, callbackPass(callbackResult));
  },
  function getState() {
    chrome.networkingPrivate.getState(
        kGuid, callbackPass(callbackResult));
  },
  function setProperties() {
    chrome.networkingPrivate.setProperties(
        kGuid, { 'GUID': kGuid }, callbackPass(callbackResult));
  },
  function createNetwork() {
    chrome.networkingPrivate.createNetwork(
        false, { 'GUID': kGuid }, callbackPass(callbackResult));
  },
  function forgetNetwork() {
    chrome.networkingPrivate.forgetNetwork(
        kGuid, callbackPass(callbackResult));
  },
  function getNetworks() {
    chrome.networkingPrivate.getNetworks(
        { networkType: 'Ethernet' }, callbackPass(callbackResult));
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
    chrome.networkingPrivate.startConnect(
        kGuid, callbackPass(callbackResult));
  },
  function startDisconnect() {
    chrome.networkingPrivate.startDisconnect(
        kGuid, callbackPass(callbackResult));
  },
  function startActivate() {
    chrome.networkingPrivate.startActivate(
        kGuid, '' /* carrier */, callbackPass(callbackResult));
  },
  function getCaptivePortalStatus() {
    chrome.networkingPrivate.getCaptivePortalStatus(
        kGuid, callbackPass(callbackResult));
  },
  function unlockCellularSim() {
    chrome.networkingPrivate.unlockCellularSim(
        kGuid, '1111', callbackPass(callbackResult));
  },
  function setCellularSimState() {
    var simState = { requirePin: true, currentPin: '1111', newPin: '1234' };
    chrome.networkingPrivate.setCellularSimState(
        kGuid, simState, callbackPass(callbackResult));
  },
  function selectCellularMobileNetwork() {
    chrome.networkingPrivate.selectCellularMobileNetwork(
        kGuid, 'fakeId', callbackPass(callbackResult));
  },
  function getGlobalPolicy() {
    chrome.networkingPrivate.getGlobalPolicy(callbackPass(callbackResult));
  }
];

var testToRun = window.location.search.substring(1);
chrome.test.runTests(availableTests.filter(function(op) {
  return op.name == testToRun;
}));

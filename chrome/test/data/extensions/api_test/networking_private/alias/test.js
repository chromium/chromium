// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that verifies that apps with only networking API alias permission
// can invoke API methods and listen to API events without encountering
// API access problems.

chrome.test.runTests([
    function onlyAliasBindingsPresent() {
      chrome.test.assertTrue(!!chrome.networking);
      chrome.test.assertTrue(!!chrome.networking.onc);

      chrome.test.assertFalse(!!chrome.networkingPrivate);
      chrome.test.succeed();
    },
    function getProperties() {
      chrome.networking.onc.getProperties(
          'stub_wifi1_guid',
          chrome.test.callbackPass(function(result) {
            chrome.test.assertEq('stub_wifi1_guid', result.GUID);
            chrome.test.assertEq(
                chrome.networking.onc.ConnectionStateType.CONNECTED,
                result.ConnectionState);
            chrome.test.assertEq('User', result.Source);
          }));
    },
    function getCellularProperties() {
      chrome.networking.onc.getProperties(
          'stub_cellular1_guid',
          chrome.test.callbackPass(function(result) {
            chrome.test.assertEq({
              Cellular: {
                ActivationState: 'NotActivated',
                AllowRoaming: false,
                AutoConnect: true,
                Family: 'GSM',
                HomeProvider: {
                  Code: '000000',
                  Country: 'us',
                  Name: 'Cellular1_Provider'
                },
                // ENS, ICCID, IMEI, MDN, MEID, and MIN are filtered for
                // networkingOnc.
                ModelID:"test_model_id",
                NetworkTechnology: 'GSM',
                RoamingState: 'Home',
                SIMLockStatus: {
                  LockEnabled: true,
                  LockType: '',
                  RetriesLeft: 3
                },
                Scanning: false,
              },
              ConnectionState: 'NotConnected',
              GUID: 'stub_cellular1_guid',
              IPAddressConfigType: chrome.networking.onc.IPConfigType.DHCP,
              Metered: true,
              TrafficCounterResetTime: 0.0,
              Name: 'cellular1',
              NameServersConfigType: chrome.networking.onc.IPConfigType.DHCP,
              Source: 'User',
              Type: 'Cellular',
            }, result);
          }));
    },
    function changeConnectionStateAndWaitForNetworksChanged() {
      chrome.test.listenOnce(
          chrome.networking.onc.onNetworksChanged,
          function(networks) {
            chrome.test.assertEq(['stub_wifi1_guid'], networks);
          });
      chrome.networking.onc.startDisconnect(
          'stub_wifi1_guid',
          chrome.test.callbackPass(function() {}));
    },
    function verifyConnectionStateChanged() {
      chrome.networking.onc.getProperties(
          'stub_wifi1_guid',
          chrome.test.callbackPass(function(result) {
            chrome.test.assertEq('stub_wifi1_guid', result.GUID);
            chrome.test.assertFalse(
                result.ConnectionState ==
                    chrome.networking.onc.ConnectionStateType.CONNECTED);
          }));
    },
    function verifyNoAccessToNetworkingPrivateOnlyMethods() {
      var expectedError = 'Requires networkingPrivate API access.';
      chrome.networking.onc.getVisibleNetworks('All',
          chrome.test.callbackFail(expectedError));
      chrome.networking.onc.getEnabledNetworkTypes(
          chrome.test.callbackFail(expectedError));
    }
]);

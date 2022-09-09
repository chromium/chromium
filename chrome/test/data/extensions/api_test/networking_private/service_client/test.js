// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The expectations in this test are for the ServiceClient implementation.
// Note: ServiceClient currently only implements WiFi networks. See
// networking_private_service_client_apitest.cc for more info.

var callbackPass = chrome.test.callbackPass;
var callbackFail = chrome.test.callbackFail;
var assertTrue = chrome.test.assertTrue;
var assertFalse = chrome.test.assertFalse;
var assertEq = chrome.test.assertEq;

var privateHelpers = {
  // Watches for the states |expectedStates| in reverse order. If all states
  // were observed in the right order, succeeds and calls |done|. If any
  // unexpected state is observed, fails.
  watchForStateChanges: function(network, expectedStates, done) {
    var self = this;
    var collectProperties = function(properties) {
      var finishTest = function() {
        chrome.networkingPrivate.onNetworksChanged.removeListener(
            self.onNetworkChange);
        done();
      };
      if (expectedStates.length > 0) {
        var expectedState = expectedStates.pop();
        assertEq(expectedState, properties.ConnectionState);
        if (expectedStates.length == 0)
          finishTest();
      }
    };
    this.onNetworkChange = function(changes) {
      assertEq([network], changes);
      chrome.networkingPrivate.getProperties(
          network,
          callbackPass(collectProperties));
    };
    chrome.networkingPrivate.onNetworksChanged.addListener(
        this.onNetworkChange);
  },
  listListener: function(expected, done) {
    var self = this;
    this.listenForChanges = function(list) {
      assertEq(expected, list);
      chrome.networkingPrivate.onNetworkListChanged.removeListener(
          self.listenForChanges);
      done();
    };
  },
  watchForCaptivePortalState: function(expectedGuid,
                                       expectedState,
                                       done) {
    var self = this;
    this.onPortalDetectionCompleted = function(guid, state) {
      assertEq(expectedGuid, guid);
      assertEq(expectedState, state);
      chrome.networkingPrivate.onPortalDetectionCompleted.removeListener(
          self.onPortalDetectionCompleted);
      done();
    };
    chrome.networkingPrivate.onPortalDetectionCompleted.addListener(
        self.onPortalDetectionCompleted);
  }
};

var availableTests = [
  function startConnect() {
    chrome.networkingPrivate.startConnect("stub_wifi2_guid", callbackPass());
  },
  function startDisconnect() {
    // Must connect to a network before we can disconnect from it.
    chrome.networkingPrivate.startConnect("stub_wifi2_guid", callbackPass(
      function() {
        chrome.networkingPrivate.startDisconnect("stub_wifi2_guid",
                                                 callbackPass());
      }));
  },
  function startConnectNonexistent() {
    chrome.networkingPrivate.startConnect(
      "nonexistent_path",
      callbackFail("Error.InvalidNetworkGuid"));
  },
  function startDisconnectNonexistent() {
    chrome.networkingPrivate.startDisconnect(
      "nonexistent_path",
      callbackFail("Error.InvalidNetworkGuid"));
  },
  function startGetPropertiesNonexistent() {
    chrome.networkingPrivate.getProperties(
      "nonexistent_path",
      callbackFail("Error.InvalidNetworkGuid"));
  },
  function createNetwork() {
    chrome.networkingPrivate.createNetwork(
      false,  // shared
      { "Type": "WiFi",
        "GUID": "ignored_guid",
        "WiFi": {
          "SSID": "wifi_created",
          "Security": "WEP-PSK"
        }
      },
      callbackPass(function(guid) {
        assertFalse(guid == "");
        assertFalse(guid == "ignored_guid");
        chrome.networkingPrivate.getProperties(
          guid,
          callbackPass(function(properties) {
            assertEq("WiFi", properties.Type);
            assertEq(guid, properties.GUID);
            assertEq("wifi_created", properties.WiFi.SSID);
            assertEq("WEP-PSK", properties.WiFi.Security);
          }));
      }));
  },
  function getNetworks() {
    // Test 'type' and 'configured'.
    chrome.networkingPrivate.getNetworks(
      { "networkType": "WiFi", "configured": true },
      callbackPass(function(result) {
        assertEq([{
          "Connectable": true,
          "ConnectionState": "Connected",
          "GUID": "stub_wifi1_guid",
          "Name": "wifi1",
          "Type": "WiFi",
          "WiFi": {
            "Security": "WEP-PSK",
            "SignalStrength": 40
          }
        }, {
          "Connectable": true,
          "ConnectionState": "NotConnected",
          "GUID": "stub_wifi2_guid",
          "Name": "wifi2_PSK",
          "Type": "WiFi",
          "WiFi": {
            "Security": "WPA-PSK",
            "SignalStrength": 80
          }
        }], result);

        // Test 'limit'.
        chrome.networkingPrivate.getNetworks(
          { "networkType": "All", "limit": 1 },
          callbackPass(function(result) {
            assertEq([{
              "Connectable": true,
              "ConnectionState": "Connected",
              "GUID": "stub_wifi1_guid",
              "Name": "wifi1",
              "Type": "WiFi",
              "WiFi": {
                    "Security": "WEP-PSK",
                "SignalStrength": 40
              }
            }], result);
          }));
      }));
  },
  function getVisibleNetworks() {
    chrome.networkingPrivate.getVisibleNetworks(
      "All",
      callbackPass(function(result) {
        assertEq([{
                    "Connectable": true,
                    "ConnectionState": "Connected",
                    "GUID": "stub_wifi1_guid",
                    "Name": "wifi1",
                    "Type": "WiFi",
                    "WiFi": {
                      "Security": "WEP-PSK",
                      "SignalStrength": 40
                    }
                  },
                  {
                    "Connectable": true,
                    "ConnectionState": "NotConnected",
                    "GUID": "stub_wifi2_guid",
                    "Name": "wifi2_PSK",
                    "Type": "WiFi",
                    "WiFi": {
                      "Security": "WPA-PSK",
                      "SignalStrength": 80
                    }
                  }], result);
      }));
  },
  function getVisibleNetworksWifi() {
    chrome.networkingPrivate.getVisibleNetworks(
      "WiFi",
      callbackPass(function(result) {
        assertEq([{
                    "Connectable": true,
                    "ConnectionState": "Connected",
                    "GUID": "stub_wifi1_guid",
                    "Name": "wifi1",
                    "Type": "WiFi",
                    "WiFi": {
                      "Security": "WEP-PSK",
                      "SignalStrength": 40
                    }
                  },
                  {
                    "Connectable": true,
                    "ConnectionState": "NotConnected",
                    "GUID": "stub_wifi2_guid",
                    "Name": "wifi2_PSK",
                    "Type": "WiFi",
                    "WiFi": {
                      "Security": "WPA-PSK",
                      "SignalStrength": 80
                    }
                  }
                  ], result);
      }));
  },
  function requestNetworkScan() {
    // Connected or Connecting networks should be listed first, sorted by type.
    var expected = ["stub_wifi1_guid",
                    "stub_wifi2_guid"];
    var done = chrome.test.callbackAdded();
    var listener = new privateHelpers.listListener(expected, done);
    chrome.networkingPrivate.onNetworkListChanged.addListener(
      listener.listenForChanges);
    chrome.networkingPrivate.requestNetworkScan();
  },
  function getProperties() {
    chrome.networkingPrivate.getProperties(
      "stub_wifi1_guid",
      callbackPass(function(result) {
        assertEq({ "Connectable": true,
                   "ConnectionState": "Connected",
                   "GUID": "stub_wifi1_guid",
                   "Name": "wifi1",
                   "Type": "WiFi",
                   "WiFi": {
                     "HexSSID": "7769666931", // "wifi1"
                     "SSID": "wifi1",
                     "Security": "WEP-PSK",
                     "SignalStrength": 40
                   }
                 }, result);
      }));
  },
  function getManagedProperties() {
    chrome.networkingPrivate.getManagedProperties(
      "stub_wifi2",
      callbackPass(function(result) {
        assertEq({
                   "Connectable": true,
                   "ConnectionState": "NotConnected",
                   "GUID": "stub_wifi2",
                   "Name": {
                     "Active": "wifi2_PSK",
                     "Effective": "UserPolicy",
                     "UserPolicy": "My WiFi Network"
                   },
                   "Source": "UserPolicy",
                   "Type": "WiFi",
                   "WiFi": {
                     "AutoConnect": {
                       "Active": false,
                       "UserEditable": true
                     },
                     "HexSSID": {
                       "Active": "77696669325F50534B", // "wifi2_PSK"
                       "Effective": "UserPolicy",
                       "UserPolicy": "77696669325F50534B"
                     },
                     "Frequency" : 5000,
                     "FrequencyList" : [2400, 5000],
                     "Passphrase": {
                       "Effective": "UserSetting",
                       "UserEditable": true,
                       "UserSetting": "FAKE_CREDENTIAL_VPaJDV9x"
                     },
                     "SSID": {
                       "Active": "wifi2_PSK",
                       "Effective": "UserPolicy",
                     },
                     "Security": {
                       "Active": "WPA-PSK",
                       "Effective": "UserPolicy",
                       "UserPolicy": "WPA-PSK"
                     },
                     "SignalStrength": 80,
                   }
                 }, result);
      }));
  },
  function setWiFiProperties() {
    var done = chrome.test.callbackAdded();
    var network_guid = "stub_wifi1_guid";
    chrome.networkingPrivate.getProperties(
        network_guid,
        callbackPass(function(result) {
          assertEq(network_guid, result.GUID);
          var new_properties = {
            Priority: 1,
            WiFi: {
              AutoConnect: true
            },
            IPAddressConfigType: 'Static',
            StaticIPConfig: {
              IPAddress: '1.2.3.4',
              Gateway: '0.0.0.0',
              RoutingPrefix: 1
            }
          };
          chrome.networkingPrivate.setProperties(
              network_guid,
              new_properties,
              callbackPass(function() {
                chrome.networkingPrivate.getProperties(
                    network_guid,
                    callbackPass(function(result) {
                      // Ensure that the GUID doesn't change.
                      assertEq(network_guid, result.GUID);
                      // Ensure that the properties were set.
                      assertEq(1, result['Priority']);
                      assertTrue('WiFi' in result);
                      assertTrue('AutoConnect' in result['WiFi']);
                      assertEq(true, result['WiFi']['AutoConnect']);
                      assertTrue('StaticIPConfig' in result);
                      assertEq('1.2.3.4',
                               result['StaticIPConfig']['IPAddress']);
                      assertEq('0.0.0.0', result['StaticIPConfig']['Gateway']);
                      assertEq(1, result['StaticIPConfig']['RoutingPrefix']);
                      done();
                    }));
              }));
        }));
  },
  function setVPNProperties() {
    var done = chrome.test.callbackAdded();
    var network_guid = "stub_vpn1_guid";
    chrome.networkingPrivate.getProperties(
        network_guid,
        callbackPass(function(result) {
          assertEq(network_guid, result.GUID);
          var new_properties = {
            Priority: 1,
            VPN: {
              Host: 'vpn.host1'
            }
          };
          chrome.networkingPrivate.setProperties(
              network_guid,
              new_properties,
              callbackPass(function() {
                chrome.networkingPrivate.getProperties(
                    network_guid,
                    callbackPass(function(result) {
                      // Ensure that the properties were set.
                      assertEq(1, result['Priority']);
                      assertTrue('VPN' in result);
                      assertTrue('Host' in result['VPN']);
                      assertEq('vpn.host1', result['VPN']['Host']);
                      // Ensure that the GUID doesn't change.
                      assertEq(network_guid, result.GUID);
                      done();
                    }));
              }));
        }));
  },
  function getState() {
    chrome.networkingPrivate.getState(
      "stub_wifi2_guid",
      callbackPass(function(result) {
        assertEq({
          "Connectable": true,
          "ConnectionState": "NotConnected",
          "GUID": "stub_wifi2_guid",
          "Name": "wifi2_PSK",
          "Type": "WiFi",
          "WiFi": {
            "Security": "WPA-PSK",
            "SignalStrength": 80
          }
        }, result);
      }));
  },
  function getStateNonExistent() {
    chrome.networkingPrivate.getState(
      'non_existent',
      callbackFail('Error.InvalidNetworkGuid'));
  },
  function onNetworksChangedEventConnect() {
    var network = "stub_wifi2_guid";
    var done = chrome.test.callbackAdded();
    var expectedStates = ["Connected"];
    var listener =
        new privateHelpers.watchForStateChanges(network, expectedStates, done);
    chrome.networkingPrivate.startConnect(network, callbackPass());
  },
  function onNetworksChangedEventDisconnect() {
    var network = "stub_wifi1_guid";
    var done = chrome.test.callbackAdded();
    var expectedStates = ["NotConnected"];
    var listener =
        new privateHelpers.watchForStateChanges(network, expectedStates, done);
    chrome.networkingPrivate.startDisconnect(network, callbackPass());
  },
  function onNetworkListChangedEvent() {
    // Connecting to wifi2 should set wifi1 to offline. Connected or Connecting
    // networks should be listed first, sorted by type.
    var expected = ["stub_wifi2_guid",
                    "stub_wifi1_guid"];
    var done = chrome.test.callbackAdded();
    var listener = new privateHelpers.listListener(expected, done);
    chrome.networkingPrivate.onNetworkListChanged.addListener(
      listener.listenForChanges);
    var network = "stub_wifi2_guid";
    chrome.networkingPrivate.startConnect(network, callbackPass());
  },
  function getCaptivePortalStatus() {
    var networks = [['stub_wifi1_guid', 'Offline'],
                    ['stub_wifi2_guid', 'Portal']];
    networks.forEach(function(network) {
      var guid = network[0];
      var expectedStatus = network[1];
      chrome.networkingPrivate.getCaptivePortalStatus(
        guid,
        callbackPass(function(status) {
          assertEq(expectedStatus, status);
        }));
    });
  },
  function captivePortalNotification() {
    var done = chrome.test.callbackAdded();
    var listener =
        new privateHelpers.watchForCaptivePortalState(
            'wifi_guid', 'Online', done);
    chrome.test.sendMessage('notifyPortalDetectorObservers');
  },
];

var testToRun = window.location.search.substring(1);
chrome.test.runTests(availableTests.filter(function(op) {
  return op.name == testToRun;
}));

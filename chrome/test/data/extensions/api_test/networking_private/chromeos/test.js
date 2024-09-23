// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The expectations in this test for the Chrome OS implementation. See
// networking_private_chromeos_apitest.cc for more info.

var callbackPass = chrome.test.callbackPass;
var callbackFail = chrome.test.callbackFail;
var assertTrue = chrome.test.assertTrue;
var assertFalse = chrome.test.assertFalse;
var assertEq = chrome.test.assertEq;

var ActivationStateType = chrome.networkingPrivate.ActivationStateType;
var ConnectionStateType = chrome.networkingPrivate.ConnectionStateType;
var NetworkType = chrome.networkingPrivate.NetworkType;

var kCellularGuid = 'stub_cellular1_guid';
var kDefaultPin = '1111';
var kDefaultPuk = '12345678';

var privateHelpers = {
  // networkToExpectedStatesMap is a Map string (network GUID) -> array of
  // strings (expected states).
  // For each network specified there, watches for onNetworksChanged events with
  // the specified states, in reverse order.
  // If all states were observed (in the right order), succeeds and calls
  // |done|. If any
  // unexpected state is observed, fails.
  watchForStateChanges: function(networkToExpectedStatesMap, done) {
    const networkToLastSeenState = new Map();
    const self = this;
    const collectProperties = function(network, properties) {
      const finishTest = function() {
        chrome.networkingPrivate.onNetworksChanged.removeListener(
            self.onNetworkChange);
        done();
      };
      const currentState = properties.ConnectionState;

      // Ignore if the state has not changed.
      const lastSeenState = networkToLastSeenState.get(network);
      if (lastSeenState && lastSeenState === currentState) {
        return;
      }
      networkToLastSeenState.set(network, currentState);

      const expectedStates = networkToExpectedStatesMap.get(network);
      if (!expectedStates) {
        chrome.test.fail(
            'Unexpected state change for network ' + network + ' (' + +')');
      }
      if (expectedStates.length > 0) {
        const expectedState = expectedStates.pop();
        assertEq(expectedState, currentState);
        if (expectedStates.length == 0) {
          networkToExpectedStatesMap.delete(network);
        }
      }
      if (networkToExpectedStatesMap.size == 0) {
        finishTest();
      }
    };
    this.onNetworkChange = function(changes) {
      for (let network of changes) {
        assertTrue(networkToExpectedStatesMap.has(network));
        chrome.networkingPrivate.getProperties(
            network, callbackPass(function(properties) {
              collectProperties(network, properties);
            }));
      }
    };
    chrome.networkingPrivate.onNetworksChanged.addListener(
        self.onNetworkChange);
  },
  networkListChangedListener: function(expected, done) {
    function listener(list) {
      assertEq(expected, list);
      chrome.networkingPrivate.onNetworkListChanged.removeListener(listener);
      done();
    };
    this.start = function() {
      chrome.networkingPrivate.onNetworkListChanged.addListener(listener);
    };
  },
  networksChangedListener: function(guid, test, done) {
    function listener(changes) {
      for (let c of changes) {
        if (c != guid)
          continue;
        chrome.networkingPrivate.onNetworksChanged.removeListener(listener);
        chrome.networkingPrivate.getProperties(guid, function(result) {
          if (test(result))
            done();
        });
      }
    };
    this.start = function() {
      chrome.networkingPrivate.onNetworksChanged.addListener(listener);
    };
  },
  watchForCaptivePortalState: function(expectedGuid, expectedState, done) {
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
  },
  verifyTetherNetwork: function(
      properties, expectedGuid, expectedName, expectedBatteryPercentage,
      expectedCarrier, expectedSignalStrength, expectedHasConnectedToHost) {
    //assertEq(NetworkType.Tether, properties.Type);
    assertEq(expectedGuid, properties.GUID);
    assertEq(expectedName,
             properties.Name.hasOwnProperty('Active') ? properties.Name.Active
                                                      : properties.Name);
    assertEq(expectedBatteryPercentage, properties.Tether.BatteryPercentage);
    assertEq(expectedCarrier, properties.Tether.Carrier);
    assertEq(expectedHasConnectedToHost, properties.Tether.HasConnectedToHost);
    assertEq(expectedSignalStrength, properties.Tether.SignalStrength);
  }
};

var kFailure = 'Failure';

function networkCallbackPass() {
  var callbackCompleted = chrome.test.callbackAdded();
  return function(result) {
    chrome.test.assertNoLastError();
    if (result === false || result === kFailure)
      chrome.test.fail('Failed: ' + result);
    callbackCompleted();
  };
}

var availableTests = [
  function startConnect() {
    chrome.networkingPrivate.startConnect('stub_wifi2_guid',
                                          networkCallbackPass());
  },
  function startDisconnect() {
    // Must connect to a network before we can disconnect from it.
    chrome.networkingPrivate.startConnect(
        'stub_wifi2_guid',
        callbackPass(function() {
          chrome.networkingPrivate.startDisconnect(
              'stub_wifi2_guid', networkCallbackPass());
        }));
  },
  function startActivate() {
    chrome.networkingPrivate.startActivate(
      kCellularGuid, callbackPass(function() {
        // For non Sprint networks, startActivate will delegate
        // showing the activation UI to the browser host and not
        // immediately activate the network.
        chrome.networkingPrivate.getState(
          kCellularGuid, callbackPass(function(state) {
            assertEq(ActivationStateType.NOT_ACTIVATED,
                     state.Cellular.ActivationState);
          }));
      }));
  },
  function startConnectNonexistent() {
    chrome.networkingPrivate.startConnect(
      'nonexistent_path',
      callbackFail('Error.InvalidNetworkGuid'));
  },
  function startDisconnectNonexistent() {
    chrome.networkingPrivate.startDisconnect(
      'nonexistent_path',
      callbackFail('Error.InvalidNetworkGuid'));
  },
  function startGetPropertiesNonexistent() {
    chrome.networkingPrivate.getProperties(
      'nonexistent_path',
      callbackFail('Error.InvalidNetworkGuid'));
  },
  function createNetwork() {
    chrome.networkingPrivate.createNetwork(
      false,  // shared
      { Type: NetworkType.WI_FI,
        GUID: 'some_guid',
        WiFi: {
          SSID: 'wifi_created',
          Security: 'WEP-PSK'
        }
      },
      callbackPass(function(guid) {
        assertFalse(guid == '');
        assertEq('some_guid', guid);
        chrome.networkingPrivate.getProperties(
            guid, callbackPass(function(properties) {
              assertEq(NetworkType.WI_FI, properties.Type);
              assertEq(guid, properties.GUID);
              assertEq('wifi_created', properties.WiFi.SSID);
              assertEq('WEP-PSK', properties.WiFi.Security);
            }));
      }));
  },
  function createNetworkForPolicyControlledNetwork() {
    chrome.networkingPrivate.getProperties('stub_wifi2', callbackPass(function(
        properties) {
      // Sanity check to verify there is a policy defined config for the network
      // config that will be set up in this test.
      chrome.test.assertEq('UserPolicy', properties.Source);
      chrome.test.assertEq('WiFi', properties.Type);
      chrome.test.assertEq('WPA-PSK', properties.WiFi.Security);
      chrome.test.assertEq('wifi2_PSK', properties.WiFi.SSID);

      chrome.networkingPrivate.createNetwork(false /* shared */, {
        Type: 'WiFi',
        WiFi: {
          SSID: 'wifi2_PSK',
          Passphrase: 'Fake password',
          Security: 'WPA-PSK'
        }
      }, callbackFail('NetworkAlreadyConfigured'));
    }));
  },
  function forgetNetwork() {
    var kNumNetworks = 2;
    var kTestNetworkGuid = 'stub_wifi1_guid';
    function guidExists(networks, guid) {
      for (var n of networks) {
        if (n.GUID == kTestNetworkGuid)
          return true;
      }
      return false;
    }
    var filter = {
      networkType: NetworkType.WI_FI,
      visible: true,
      configured: true
    };
    chrome.networkingPrivate.getNetworks(
        filter, callbackPass(function(networks) {
          assertEq(kNumNetworks, networks.length);
          assertTrue(guidExists(networks, kTestNetworkGuid));
          chrome.networkingPrivate.forgetNetwork(
              kTestNetworkGuid, callbackPass(function() {
                chrome.networkingPrivate.getNetworks(
                    filter, callbackPass(function(networks) {
                      assertEq(kNumNetworks - 1, networks.length);
                      assertFalse(guidExists(networks, kTestNetworkGuid));
                    }));
              }));
        }));
  },
  function forgetPolicyControlledNetwork() {
    chrome.networkingPrivate.getProperties('stub_wifi2', callbackPass(function(
        properties) {
      // Sanity check to verify there is a policy defined config for the network
      // config that will be set up in this test.
      chrome.test.assertEq('UserPolicy', properties.Source);
      chrome.test.assertEq('WiFi', properties.Type);
      chrome.test.assertEq('WPA-PSK', properties.WiFi.Security);
      chrome.test.assertEq('wifi2_PSK', properties.WiFi.SSID);

      chrome.networkingPrivate.forgetNetwork(
          'stub_wifi2', callbackFail('Error.PolicyControlled'));
    }));
  },
  function getNetworks() {
    // Test 'type' and 'configured'.
    var filter = {
      networkType: NetworkType.WI_FI,
      configured: true
    };
    chrome.networkingPrivate.getNetworks(
      filter,
      callbackPass(function(result) {
        assertEq([{
          Connectable: true,
          ConnectionState: ConnectionStateType.CONNECTED,
          GUID: 'stub_wifi1_guid',
          Name: 'wifi1',
          Priority: 0,
          Source: 'User',
          Type: NetworkType.WI_FI,
          WiFi: {
            BSSID: '00:01:02:03:04:05',
            Frequency: 2400,
            HexSSID: "7769666931",
            Security: 'WEP-PSK',
            SignalStrength: 40,
            SSID: "wifi1",
          }
        }, {
          GUID: 'stub_wifi2_guid',
          Name: 'wifi2_PSK',
          Priority: 0,
          Source: 'User',
          Type: NetworkType.WI_FI,
          WiFi: {
            BSSID: '',
            Frequency: 5000,
            HexSSID: "77696669325F50534B",
            Security: 'WPA-PSK',
            SSID: "wifi2_PSK",
          }
        }], result);

        // Test 'visible' (and 'configured').
        filter.visible = true;
        chrome.networkingPrivate.getNetworks(
          filter,
          callbackPass(function(result) {
            assertEq([{
              Connectable: true,
              ConnectionState: ConnectionStateType.CONNECTED,
              GUID: 'stub_wifi1_guid',
              Name: 'wifi1',
              Priority: 0,
              Source: 'User',
              Type: NetworkType.WI_FI,
              WiFi: {
                BSSID: '00:01:02:03:04:05',
                Frequency: 2400,
                HexSSID: "7769666931",
                Security: 'WEP-PSK',
                SignalStrength: 40,
                SSID: "wifi1",
              }
            }], result);

            // Test 'limit'.
            filter = {
              networkType: NetworkType.ALL,
              limit: 1
            };
            chrome.networkingPrivate.getNetworks(
              filter,
              callbackPass(function(result) {
                assertEq([{
                  ConnectionState: ConnectionStateType.CONNECTED,
                  Ethernet: {
                    Authentication: 'None'
                  },
                  GUID: 'stub_ethernet_guid',
                  Name: 'eth0',
                  Priority: 0,
                  Source: 'Device',
                  Type: NetworkType.ETHERNET
                }], result);
              }));
          }));
      }));
  },
  function getVisibleNetworks() {
    chrome.networkingPrivate.getVisibleNetworks(
      NetworkType.ALL,
      callbackPass(function(result) {
        assertEq([{
          ConnectionState: ConnectionStateType.CONNECTED,
          Ethernet: {
            Authentication: 'None'
          },
          GUID: 'stub_ethernet_guid',
          Name: 'eth0',
          Priority: 0,
          Source: 'Device',
          Type: NetworkType.ETHERNET
        }, {
          Connectable: true,
          ConnectionState: ConnectionStateType.CONNECTED,
          GUID: 'stub_wifi1_guid',
          Name: 'wifi1',
          Priority: 0,
          Source: 'User',
          Type: NetworkType.WI_FI,
          WiFi: {
            BSSID: '00:01:02:03:04:05',
            Frequency: 2400,
            HexSSID: "7769666931",
            Security: 'WEP-PSK',
            SignalStrength: 40,
            SSID: "wifi1",
          }
        }, {
          ConnectionState: ConnectionStateType.CONNECTED,
          GUID: 'stub_vpn1_guid',
          Name: 'vpn1',
          Priority: 0,
          Source: 'User',
          Type: NetworkType.VPN,
          VPN: {
            Type: 'OpenVPN'
          }
        }, {
          ConnectionState: ConnectionStateType.NOT_CONNECTED,
          GUID: 'stub_vpn2_guid',
          Name: 'vpn2',
          Priority: 0,
          Source: 'User',
          Type: NetworkType.VPN,
          VPN: {
            ThirdPartyVPN: {
              ExtensionID: 'third_party_provider_extension_id'
            },
            Type: 'ThirdPartyVPN'
          }
        }, {
          Connectable: true,
          ConnectionState: ConnectionStateType.NOT_CONNECTED,
          GUID: 'stub_wifi2_guid',
          Name: 'wifi2_PSK',
          Priority: 0,
          Source: 'User',
          Type: NetworkType.WI_FI,
          WiFi: {
            BSSID: '',
            Frequency: 5000,
            HexSSID: "77696669325F50534B",
            Security: 'WPA-PSK',
            SignalStrength: 80,
            SSID: "wifi2_PSK",
          }
        }], result);
      }));
  },
  function getVisibleNetworksWifi() {
    chrome.networkingPrivate.getVisibleNetworks(
      NetworkType.WI_FI,
      callbackPass(function(result) {
        assertEq([{
          Connectable: true,
          ConnectionState: ConnectionStateType.CONNECTED,
          GUID: 'stub_wifi1_guid',
          Name: 'wifi1',
          Priority: 0,
          Source: 'User',
          Type: NetworkType.WI_FI,
          WiFi: {
            BSSID: '00:01:02:03:04:05',
            Frequency: 2400,
            HexSSID: "7769666931",
            Security: 'WEP-PSK',
            SignalStrength: 40,
            SSID: "wifi1",
          }
        }, {
          Connectable: true,
          ConnectionState: ConnectionStateType.NOT_CONNECTED,
          GUID: 'stub_wifi2_guid',
          Name: 'wifi2_PSK',
          Priority: 0,
          Source: 'User',
          Type: NetworkType.WI_FI,
          WiFi: {
            BSSID: '',
            Frequency: 5000,
            HexSSID: "77696669325F50534B",
            Security: 'WPA-PSK',
            SignalStrength: 80,
            SSID: "wifi2_PSK",
          }
        }], result);
      }));
  },
  function enabledNetworkTypesDisable() {
    chrome.networkingPrivate.getEnabledNetworkTypes(function(types) {
      assertTrue(types.indexOf('WiFi') >= 0);
      var listener = callbackPass(function() {
        chrome.networkingPrivate.onDeviceStateListChanged.removeListener(
          listener);
        chrome.networkingPrivate.getEnabledNetworkTypes(
          callbackPass(function(types2) {
            assertFalse(types2.indexOf('WiFi') >= 0);
          }));
      });
      chrome.networkingPrivate.onDeviceStateListChanged.addListener(listener);
      chrome.networkingPrivate.disableNetworkType('WiFi');
    });
  },

  function enabledNetworkTypesEnable() {
    chrome.networkingPrivate.getEnabledNetworkTypes(function(types) {
      assertFalse(types.indexOf('WiFi') >= 0);
      var listener = callbackPass(function() {
        chrome.networkingPrivate.onDeviceStateListChanged.removeListener(
          listener);
        chrome.networkingPrivate.getEnabledNetworkTypes(
          callbackPass(function(types2) {
            assertTrue(types2.indexOf('WiFi') >= 0);
          }));
      });
      chrome.networkingPrivate.onDeviceStateListChanged.addListener(listener);
      chrome.networkingPrivate.enableNetworkType('WiFi');
    });
  },

  function getDeviceStates() {
    chrome.networkingPrivate.getDeviceStates(callbackPass(function(result) {
      assertEq([
        {Scanning: false, State: 'Enabled', Type: 'Ethernet'},
        {Scanning: false, State: 'Enabled', Type: 'WiFi',
         ManagedNetworkAvailable: false},
        {State: 'Uninitialized', SIMPresent: true,
         SIMLockStatus: {LockEnabled: true, LockType: '', RetriesLeft: 3},
         Type: 'Cellular' },
      ],
               result);
    }));
  },

  function getDeviceStatesLacros() {
    chrome.networkingPrivate.getDeviceStates(callbackPass(function(result) {
      // Tether scanning value is flaky, ignore it in this test
      tetherIdx = result.findIndex((element) => element.Type === 'Tether');
      assertTrue(tetherIdx > -1);
      delete result[tetherIdx].Scanning;

      assertEq(
          [
            {Scanning: false, State: 'Enabled', Type: 'Ethernet'},
            {
              ManagedNetworkAvailable: false,
              Scanning: false,
              State: 'Enabled',
              Type: 'WiFi'
            },
            {State: 'Enabled', Type: 'Tether'},
            {State: 'Enabled', Type: 'Cellular'},
          ],
          result);
    }));
  },

  function requestNetworkScan() {
    // Connected or Connecting networks should be listed first, sorted by type.
    var expected = ['stub_ethernet_guid',
                    'stub_wifi1_guid',
                    'stub_vpn1_guid',
                    'stub_vpn2_guid',
                    'stub_wifi2_guid'];
    var done = chrome.test.callbackAdded();
    var listener =
        new privateHelpers.networkListChangedListener(expected, done);
    listener.start();
    chrome.networkingPrivate.requestNetworkScan();
  },
  function requestNetworkScanCellular() {
    var done = chrome.test.callbackAdded();
    var listener = new privateHelpers.networksChangedListener(
        kCellularGuid, function(result) {
          var cellular = result.Cellular;
          return cellular && cellular.FoundNetworks &&
              cellular.FoundNetworks[0].Status == 'available';
        }, done);
    listener.start();
    chrome.networkingPrivate.requestNetworkScan('Cellular');
  },
  function getProperties() {
    chrome.networkingPrivate.getProperties(
      'stub_wifi1_guid',
      callbackPass(function(result) {
        assertEq({
          Connectable: true,
          ConnectionState: ConnectionStateType.CONNECTED,
          GUID: 'stub_wifi1_guid',
          IPAddressConfigType: chrome.networkingPrivate.IPConfigType.STATIC,
          IPConfigs: [{
            Gateway: '0.0.0.1',
            IPAddress: '0.0.0.0',
            RoutingPrefix: 0,
            Type: 'IPv4'
          }],
          MacAddress: '00:11:22:AA:BB:CC',
          Name: 'wifi1',
          NameServersConfigType: chrome.networkingPrivate.IPConfigType.DHCP,
          Source: 'User',
          StaticIPConfig: {
            IPAddress: '1.2.3.4',
            Gateway: '0.0.0.0',
            RoutingPrefix: 1,
            Type: 'IPv4'
          },
          Type: NetworkType.WI_FI,
          WiFi: {
            BSSID: '00:01:02:03:04:05',
            HexSSID: '7769666931', // 'wifi1'
            Frequency: 2400,
            FrequencyList: [2400],
            SSID: 'wifi1',
            Security: 'WEP-PSK',
            SignalStrength: 40,
          }
        }, result);
      }));
  },
  function getPropertiesCellular() {
    chrome.networkingPrivate.getProperties(
      kCellularGuid,
      callbackPass(function(result) {
        assertEq({
          Cellular: {
            ActivationState: ActivationStateType.NOT_ACTIVATED,
            AllowRoaming: false,
            AutoConnect: true,
            Family: 'GSM',
            HomeProvider: {
              Code: '000000',
              Country: 'us',
              Name: 'Cellular1_Provider'
            },
            ESN: "test_esn",
            ICCID: "test_iccid",
            IMEI: "test_imei",
            MDN: "test_mdn",
            MEID: "test_meid",
            MIN: "test_min",
            ModelID:"test_model_id",
            NetworkTechnology: 'GSM',
            RoamingState: 'Home',
            SIMLockStatus: {LockEnabled: true, LockType: '', RetriesLeft: 3},
            Scanning: false,
            LastGoodAPN: {
              AccessPointName: "default_apn",
              ApnTypes: ["Default"],
              Authentication: "CHAP",
              LocalizedName: "localized test apn",
              Name: "default_apn",
              Username: "user name",
              Password: "password",
              Source: "Modb",
            },
          },
          ConnectionState: ConnectionStateType.NOT_CONNECTED,
          GUID: kCellularGuid,
          IPAddressConfigType: chrome.networkingPrivate.IPConfigType.DHCP,
          Metered: true,
          TrafficCounterResetTime: 0.0,
          Name: 'cellular1',
          NameServersConfigType: chrome.networkingPrivate.IPConfigType.DHCP,
          Source: 'User',
          Type: NetworkType.CELLULAR,
        }, result);
      }));
  },
  function getPropertiesCellularDefault() {
    filter = {networkType: NetworkType.CELLULAR, limit: 1};
    chrome.networkingPrivate.getNetworks(
      filter, callbackPass(function(networks) {
        assertEq(1, networks.length);
        var guid = networks[0].GUID;
        chrome.networkingPrivate.getProperties(
          guid, callbackPass(function(result) {
            assertEq({
              Cellular: {
                AllowRoaming: false,
                ESN: "test_esn",
                Family: 'GSM',
                HomeProvider: {
                  Code: '000000',
                  Country: 'us',
                  Name: 'Cellular1_Provider',
                },
                ICCID: "test_iccid",
                IMEI: "test_imei",
                MDN: "test_mdn",
                MEID: "test_meid",
                MIN: "test_min",
                ModelID:"test_model_id",
                SIMLockStatus: {
                  LockEnabled: true,
                  LockType: '',
                  RetriesLeft: 3,
                },
                Scanning: false,
                SignalStrength: 0,
              },
              Connectable: false,
              ConnectionState: ConnectionStateType.NOT_CONNECTED,
              GUID: guid,
              IPAddressConfigType: chrome.networkingPrivate.IPConfigType.DHCP,
              Name: '',
              NameServersConfigType: chrome.networkingPrivate.IPConfigType.DHCP,
              Priority: 0,
              Source: 'None',
              Type: NetworkType.CELLULAR,
            }, result);
          }))}));
  },
  function getManagedProperties() {
    chrome.networkingPrivate.getManagedProperties(
      'stub_wifi2',
      callbackPass(function(result) {
        assertEq({
          ConnectionState: ConnectionStateType.NOT_CONNECTED,
          GUID: 'stub_wifi2',
          IPAddressConfigType: {
            Active: 'DHCP',
            Effective: 'UserPolicy'
          },
          Name: {
            Active: 'wifi2_PSK',
            Effective: 'UserPolicy',
            UserPolicy: 'My WiFi Network'
          },
          NameServersConfigType: {
            Active: 'DHCP',
            Effective: 'UserPolicy'
          },
          ProxySettings: {
            Type: {
              Active: 'Direct',
              Effective: 'UserPolicy',
              UserPolicy: 'Direct'
            }
          },
          Source: 'UserPolicy',
          Type: NetworkType.WI_FI,
          WiFi: {
            AutoConnect: {
              Effective: 'UserPolicy',
              UserEditable: true,
              UserPolicy: false
            },
            Frequency: 5000,
            FrequencyList: [2400, 5000],
            HexSSID: {
              Active: '77696669325F50534B', // 'wifi2_PSK'
              Effective: 'UserPolicy',
              UserPolicy: '77696669325F50534B'
            },
            HiddenSSID: {
              Active: false,
              Effective: 'UserPolicy',
              UserPolicy: false,
            },
            Passphrase: {
              Effective: 'UserSetting',
              UserEditable: true,
              UserPolicy: 'FAKE_CREDENTIAL_VPaJDV9x',
              UserSetting: 'FAKE_CREDENTIAL_VPaJDV9x'
            },
            SSID: {
              Active: 'wifi2_PSK',
              Effective: 'UserPolicy',
            },
            Security: {
              Active: 'WPA-PSK',
              Effective: 'UserPolicy',
              UserPolicy: 'WPA-PSK'
            },
            SignalStrength: 80,
          }
        }, result);
      }));
  },
  function setCellularProperties() {
    var network_guid = kCellularGuid;
    chrome.networkingPrivate.getProperties(
        network_guid,
        callbackPass(function(result) {
          assertEq(network_guid, result.GUID);
          var new_properties = {
            Priority: 1
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
                    }));
              }));
        }));
  },
  function setWiFiProperties() {
    var network_guid = 'stub_wifi1_guid';
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
                    }));
              }));
        }));
  },
  function setVPNProperties() {
    var network_guid = 'stub_vpn1_guid';
    chrome.networkingPrivate.getProperties(
        network_guid,
        callbackPass(function(result) {
          assertEq(network_guid, result.GUID);
          var new_properties = {
            Priority: 1,
            Type: 'VPN',
            VPN: {
              Host: 'vpn.host1',
              Type: 'OpenVPN',
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
                    }));
              }));
        }));
  },
  function getState() {
    chrome.networkingPrivate.getState(
      'stub_wifi2_guid',
      callbackPass(function(result) {
        assertEq({
          Connectable: true,
          ConnectionState: ConnectionStateType.NOT_CONNECTED,
          GUID: 'stub_wifi2_guid',
          Name: 'wifi2_PSK',
          Priority: 0,
          Source: 'User',
          Type: NetworkType.WI_FI,
          WiFi: {
            BSSID: '',
            Frequency: 5000,
            HexSSID: "77696669325F50534B",
            Security: 'WPA-PSK',
            SignalStrength: 80,
            SSID: "wifi2_PSK",
          }
        }, result);
      }));
  },
  function getStateNonExistent() {
    chrome.networkingPrivate.getState(
      'non_existent',
      callbackFail('Error.InvalidNetworkGuid'));
  },
  function getErrorState() {
    // Both getState and getProperties should have ErrorState set.
    chrome.networkingPrivate.getState(
        'stub_wifi1_guid', callbackPass(function(result) {
          assertEq('TestErrorState', result.ErrorState);
          chrome.networkingPrivate.getProperties(
              'stub_wifi1_guid', callbackPass(function(result2) {
                assertEq('TestErrorState', result2.ErrorState);
              }));
        }));
  },
  function onNetworksChangedEventConnect() {
    var network = 'stub_wifi2_guid';
    var done = chrome.test.callbackAdded();
    var listener = new privateHelpers.watchForStateChanges(
        new Map([
          ['stub_wifi1_guid', [ConnectionStateType.NOT_CONNECTED]],
          [network, [ConnectionStateType.CONNECTED]]
        ]),
        done);
    chrome.networkingPrivate.startConnect(network, networkCallbackPass());
  },
  function onNetworksChangedEventDisconnect() {
    var network = 'stub_wifi1_guid';
    var done = chrome.test.callbackAdded();
    var listener = new privateHelpers.watchForStateChanges(
        new Map([[network, [ConnectionStateType.NOT_CONNECTED]]]), done);
    chrome.networkingPrivate.startDisconnect(network, networkCallbackPass());
  },
  function onNetworkListChangedEvent() {
    // Connecting to wifi2 should set wifi1 to offline. Connected or Connecting
    // networks should be listed first, sorted by type.
    var expected = ['stub_ethernet_guid',
                    'stub_vpn1_guid',
                    'stub_wifi2_guid',
                    'stub_wifi1_guid',
                    'stub_vpn2_guid'];
    var done = chrome.test.callbackAdded();
    var listener =
        new privateHelpers.networkListChangedListener(expected, done);
    listener.start();
    var network = 'stub_wifi2_guid';
    chrome.networkingPrivate.startConnect(network, networkCallbackPass());
  },
  function onDeviceStateListChangedEvent() {
    var listener = callbackPass(function() {
      chrome.networkingPrivate.onDeviceStateListChanged.removeListener(
          listener);
    });
    chrome.networkingPrivate.onDeviceStateListChanged.addListener(listener);
    chrome.networkingPrivate.disableNetworkType('WiFi');
  },
  function onDeviceScanningChangedEvent() {
    // Requesting a scan should trigger a device state list changed event when
    // the scan completes.
    var listener = callbackPass(function() {
      chrome.networkingPrivate.onDeviceStateListChanged.removeListener(
          listener);
    });
    chrome.networkingPrivate.onDeviceStateListChanged.addListener(listener);
    chrome.networkingPrivate.requestNetworkScan('Cellular');
  },
  function onCertificateListsChangedEvent() {
    chrome.test.listenOnce(
        chrome.networkingPrivate.onCertificateListsChanged, function() {});
    chrome.test.sendMessage('eventListenerReady');
  },
  function getCaptivePortalStatus() {
    var networks = [['stub_ethernet_guid', 'Online'],
                    ['stub_wifi1_guid', 'Offline'],
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
            'stub_wifi1_guid', 'Portal', done);
    chrome.test.sendMessage('notifyPortalDetectorObservers');
  },
  function unlockCellularSim() {
    var incorrectPin = '2222';
    // Try with incorrect PIN, expect failure.
    chrome.networkingPrivate.unlockCellularSim(
        kCellularGuid, incorrectPin, '',
        callbackFail('incorrect-pin', function() {
          // Try with correct PIN, expect success.
          chrome.networkingPrivate.unlockCellularSim(
              kCellularGuid, kDefaultPin, '', networkCallbackPass());
        }));
  },
  function setCellularSimState() {
    var newPin = '6666';
    var simState = {requirePin: true, currentPin: kDefaultPin, newPin: newPin};
    // Test setting 'requirePin' and 'newPin'.
    chrome.networkingPrivate.getProperties(
        kCellularGuid, callbackPass(function(result) {
          // Ensure the SIM is initially unlocked.
          assertTrue(result.Cellular.SIMLockStatus == undefined ||
                     result.Cellular.SIMLockStatus.LockType == '');
          chrome.networkingPrivate.setCellularSimState(
              kCellularGuid, simState, callbackPass(function() {
                chrome.networkingPrivate.getProperties(
                    kCellularGuid, callbackPass(function(result) {
                      // The SIM should still be unlocked.
                      assertEq('', result.Cellular.SIMLockStatus.LockType);
                      // Ensure SIM locking is enabled.
                      assertTrue(result.Cellular.SIMLockStatus.LockEnabled);
                      // Ensure the new pin is set by using the new PIN
                      // to change the PIN back.
                      simState.currentPin = newPin;
                      simState.newPin = kDefaultPin;
                      chrome.networkingPrivate.setCellularSimState(
                          kCellularGuid, simState, networkCallbackPass());
                    }));
              }));
        }));
  },
  function selectCellularMobileNetwork() {
    chrome.networkingPrivate.getProperties(
        kCellularGuid, callbackPass(function(result) {
          // Ensure that there are two found networks and the first is selected.
          assertTrue(!!result.Cellular.FoundNetworks);
          assertTrue(result.Cellular.FoundNetworks.length >= 2);
          assertTrue(result.Cellular.FoundNetworks[0].Status == 'current');
          assertTrue(result.Cellular.FoundNetworks[1].Status == 'available');
          // Select the second network
          var secondNetworkId = result.Cellular.FoundNetworks[1].NetworkId;
          chrome.networkingPrivate.selectCellularMobileNetwork(
              kCellularGuid, secondNetworkId, callbackPass(function() {
                chrome.networkingPrivate.getProperties(
                    kCellularGuid, callbackPass(function(result) {
                      // Ensure that the second network is selected.
                      assertTrue(!!result.Cellular.FoundNetworks);
                      assertTrue(result.Cellular.FoundNetworks.length >= 2);
                      assertEq(
                          'available', result.Cellular.FoundNetworks[0].Status);
                      assertEq(
                          'current', result.Cellular.FoundNetworks[1].Status);
                    }));
              }));
        }));
  },
  function cellularSimPuk() {
    var newPin = '6666';
    var incorrectPin = '2222';
    var incorrectPuk = '22222222';
    var unlockFailFunc = function(nextFunc) {
      chrome.networkingPrivate.unlockCellularSim(
          kCellularGuid, incorrectPin, '',
          callbackFail('incorrect-pin', nextFunc));
    };
    // Try with incorrect PIN three times, SIM should become PUK locked.
    unlockFailFunc(unlockFailFunc(unlockFailFunc(function() {
      // Ensure the SIM is PUK locked.
      chrome.networkingPrivate.getProperties(
          kCellularGuid, callbackPass(function(result) {
            assertEq('sim-puk', result.Cellular.SIMLockStatus.LockType);
            // Try to unlock with an incorrect PUK, expect failure.
            chrome.networkingPrivate.unlockCellularSim(
                kCellularGuid, newPin, incorrectPuk,
                callbackFail('incorrect-pin', function() {
                  // Try with the correct PUK, expect success.
                  chrome.networkingPrivate.unlockCellularSim(
                      kCellularGuid, newPin, kDefaultPuk,
                      callbackPass(function() {
                        // Set state with the new PIN, expect success.
                        var simState = {requirePin: true, currentPin: newPin};
                        chrome.networkingPrivate.setCellularSimState(
                            kCellularGuid, simState, networkCallbackPass());
                      }));
                }));
          }));
    })));
  },
  function getGlobalPolicy() {
    chrome.networkingPrivate.getGlobalPolicy(callbackPass(function(result) {
      assertEq({
        AllowOnlyPolicyNetworksToAutoconnect: true,
        AllowOnlyPolicyNetworksToConnect: false,
      }, result);
    }));
  },
  function getTetherNetworks() {
    chrome.networkingPrivate.getNetworks(
        {networkType: 'Tether'},
        callbackPass(function(tetherNetworks) {
          assertEq(2, tetherNetworks.length);
          privateHelpers.verifyTetherNetwork(tetherNetworks[0], 'tetherGuid1',
              'tetherName1', 50, 'tetherCarrier1', 75, true);
          privateHelpers.verifyTetherNetwork(tetherNetworks[1], 'tetherGuid2',
              'tetherName2', 75, 'tetherCarrier2', 100, false);
        }));
  },
  function getTetherNetworkProperties() {
    chrome.networkingPrivate.getProperties(
        'tetherGuid1',
        callbackPass(function(tetherNetwork) {
          privateHelpers.verifyTetherNetwork(tetherNetwork, 'tetherGuid1',
              'tetherName1', 50, 'tetherCarrier1', 75, true);
        }));
  },
  function getTetherNetworkManagedProperties() {
    chrome.networkingPrivate.getManagedProperties(
        'tetherGuid1',
        callbackPass(function(tetherNetwork) {
          privateHelpers.verifyTetherNetwork(tetherNetwork, 'tetherGuid1',
              'tetherName1', 50, 'tetherCarrier1', 75, true);
        }));
  },
  function getTetherNetworkState() {
    chrome.networkingPrivate.getState(
        'tetherGuid1',
        callbackPass(function(tetherNetwork) {
          privateHelpers.verifyTetherNetwork(tetherNetwork, 'tetherGuid1',
              'tetherName1', 50, 'tetherCarrier1', 75, true);
        }));
  },
  function getCertificateLists() {
    chrome.networkingPrivate.getCertificateLists(
        callbackPass(function(certificateLists) {
          assertEq(1, certificateLists.serverCaCertificates.length);
          assertEq(0, certificateLists.userCertificates.length);
        }));
  },
];

chrome.test.getConfig(function(config) {
  var args = JSON.parse(config.customArg);
  var tests = availableTests.filter(function(op) {
    return args.test == op.name;
  });
  if (tests.length !== 1) {
    chrome.test.notifyFail('Test not found ' + args.test);
    return;
  }

  chrome.test.runTests(tests);
});

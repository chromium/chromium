// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var selectedTest = location.hash.slice(1);

// The below *Failures() function are called with no configuration created.
function createConfigFailures() {
  chrome.vpnProvider.createConfig('some config name', function() {
    chrome.test.assertEq(chrome.runtime.lastError, undefined);
    chrome.vpnProvider.createConfig('some config name', function() {
      chrome.test.assertEq('Name not unique.',
                           chrome.runtime.lastError.message);
      chrome.vpnProvider.createConfig('', function() {
        chrome.test.assertEq('Empty name not supported.',
                             chrome.runtime.lastError.message);
        chrome.test.succeed();
      });
    });
  });
}

function destroyConfigFailures() {
  chrome.vpnProvider.destroyConfig('nonexistent', function() {
    chrome.test.assertEq('Unauthorized access.',
                         chrome.runtime.lastError.message);
    chrome.test.succeed();
  });
}

function setParameterFailures() {
  var errors = [
    "Address CIDR sanity check failed.",
    "DNS server IP sanity check failed.",
    // If none of the above errors are thrown, the API will throw the below
    // error because of the missing 'connected' message from the platform.
    "Unauthorized access."
  ];
  // First entry in each element is an index into the |errors| array.
  // Second entry is the input to the address entry in parameters passed to
  // chrome.vpnProvider.setParameters API.
  // Third entry is the input to the dnsServers entry in parameters passed to
  // chrome.vpnProvider.setParameters API.
  var argsList = [
    [0, "1+++", ""],              // + not allowed
    [0, "1", ""],                 // 3 dots and separator missing
    [0, "1..", ""],               // A dot and separator missing
    [0, "1...", ""],              // Separator missing
    [0, "1.../", ""],             // No digit after separator in address
    [1, "1.../0", ""],            // Address passes sanity check, DNS incorrect
    [1, "1.../0", "1.../"],       // DNS is not CIDR
    [2, "1.../0", "1..."],        // Passes sanity checks for IPv4
    [0, ".../", "..."],           // Address has no digits
    [0, "0.../", "..."],          // Address has no digits post separator
    [1, "0.../0", "..."],         // Address passes sanity check, DNS incorrect
    [2, "0.../0", "...0"],        // Passes sanity checks for IPv4
    [0, "1...:::/1279abe", ""],   // : not allowed for ipv4
    [0, "1.../1279abcde", ""],    // Hex not allowed after separator
    [0, "1...abcde/1279", ""],    // Hex not allowed in ipv4
    [1, "1.../1279", ""],         // Address passes sanity check, DNS incorrect
    [2, "1.../1279", "1..."],     // Passes sanity checks for IPv4
    [0, "1--++", ""],             // + and - not supported
    [0, "1.1.1.1", ""],           // Missing separator
    [0, "1.1.1.1/", ""],          // No digits after separator in address
    [1, "1.1.1.1/1", ""],         // Address passes sanity check, DNS incorrect
    [2, "1.1.1.1/1", "1.1.1.1"],  // Passes sanity checks for IPv4
    [0, "1.1.1./e", "1.1.1."],    // Hex not okay in ipv4
    [2, "1.1.1./0", "1.1.1."],    // Passes sanity checks for IPv4
    [1, "1.../1279", "..."],      // No digits in DNS
    [1, "1.../1279", "e..."],     // Hex not allowed in ipv4
    [2, "1.../1279", "4..."],     // Passes sanity checks for IPv4
  ];

  function recurse(index) {
    if (index >= argsList.length) {
      chrome.test.succeed();
      return;
    }
    var args = argsList[index];
    var error = errors[args[0]];
    var params = {
      address: args[1],
      exclusionList: [],
      inclusionList: [],
      dnsServers: [args[2]]
    };
    chrome.vpnProvider.setParameters(params, function() {
      chrome.test.assertEq(error, chrome.runtime.lastError.message,
                           'Test ' + index + ' failed');
      recurse(index + 1);
    });
  }

  recurse(0);
}

function sendPacketFailures() {
  var data1 = new ArrayBuffer(1);
  chrome.vpnProvider.sendPacket(data1, function() {
    chrome.test.assertEq('Unauthorized access.',
                         chrome.runtime.lastError.message);
    chrome.test.succeed();
  });
}

function notifyConnectionStateChangedFailures() {
  chrome.vpnProvider.notifyConnectionStateChanged('connected', function() {
    chrome.test.assertEq('Unauthorized access.',
                         chrome.runtime.lastError.message);
    chrome.vpnProvider.notifyConnectionStateChanged('failure', function() {
      chrome.test.assertEq('Unauthorized access.',
                           chrome.runtime.lastError.message);
      chrome.test.succeed();
    });
  });
}

function createDestroyRace() {
  chrome.vpnProvider.createConfig('test-config', function() {});
  chrome.vpnProvider.destroyConfig('test-config', function() {
    // Depending upon who wins the race either destroyConfig succeeds or a
    // 'Pending create.' error is returned.
    if (chrome.runtime.lastError) {
      chrome.test.assertEq('Pending create.', chrome.runtime.lastError.message);
    }
    chrome.test.succeed();
  });
}

function destroyCreateRace() {
  chrome.vpnProvider.createConfig('test-config1', function() {
    chrome.test.assertEq(chrome.runtime.lastError, undefined);
    chrome.vpnProvider.destroyConfig('test-config1', function() {});
    chrome.vpnProvider.createConfig('test-config1', function() {
      chrome.test.assertEq(chrome.runtime.lastError, undefined);
      chrome.test.succeed();
    });
  });
}

var testRoutines = {
  comboSuite: function() {
    var tests = [
      createConfigFailures,
      destroyConfigFailures,
      setParameterFailures,
      sendPacketFailures,
      notifyConnectionStateChangedFailures,
      createDestroyRace,
      destroyCreateRace
    ];
    chrome.test.runTests(tests);
  },
  createConfigSuccess: function() {
    chrome.vpnProvider.createConfig('testconfig', function() {
      chrome.test.assertEq(chrome.runtime.lastError, undefined);
      chrome.test.succeed();
    });
  },
  createConfigConnectForBind: function() {
    chrome.vpnProvider.onPlatformMessage.addListener(function(config_name,
                                                              message, error) {
      if (message === 'connected') {
        chrome.test.assertEq(config_name, 'testconfig');
        chrome.vpnProvider.notifyConnectionStateChanged('connected', () => {
          chrome.test.succeed();
        });
      } else {
        chrome.test.assertEq(message, 'disconnected');
        chrome.test.succeed();
      }
    });
    chrome.vpnProvider.createConfig('testconfig', function() {
      chrome.test.assertEq(chrome.runtime.lastError, undefined);
      chrome.test.succeed();
    });
  },
  createConfigConnectAndDisconnect: function() {
    // The test sets up a set of listeners and creates a config.
    // The created config is connected to by the C++ side, which initiates the
    // VPN connection routine. When the routine is complete, a few data packets
    // are exchanged between the C++ side and the JS side. After this the C++
    // side sends a disconnect message which ends the test.
    var expectDisconnect = false;
    chrome.vpnProvider.onPacketReceived.addListener(function(data) {
      chrome.test.assertEq(chrome.runtime.lastError, undefined);
      // The variable packet contains the string 'deadbeef'.
      var packet = new Uint8Array([100, 101, 97, 100, 98, 101, 101, 102]);
      chrome.test.assertEq(packet, new Uint8Array(data));
      chrome.test.succeed();
    });
    var onNotifyComplete = function() {
      chrome.test.assertEq(chrome.runtime.lastError, undefined);
      chrome.vpnProvider.sendPacket(new ArrayBuffer(0), function() {
        chrome.test.assertEq(chrome.runtime.lastError.message,
                             "Can't send an empty packet.");
        // The variable packet contains the string 'feebdaed'.
        var packet = new Uint8Array([102, 101, 101, 98, 100, 97, 101, 100]);
        chrome.vpnProvider.sendPacket(packet.buffer, function() {
          chrome.test.assertEq(chrome.runtime.lastError, undefined);
          expectDisconnect = true;
          chrome.test.succeed();
        });
      });
    };
    var onSetParameterComplete = function() {
      chrome.test.assertEq(chrome.runtime.lastError, undefined);
      chrome.vpnProvider.notifyConnectionStateChanged('connected',
                                                      onNotifyComplete);
    };
    chrome.vpnProvider.onPlatformMessage.addListener(function(config_name,
                                                              message, error) {
      chrome.test.assertEq(config_name, 'testconfig');
      if (expectDisconnect) {
        chrome.test.assertEq(message, 'disconnected');
        // After disconnect authorization failures should happen.
        chrome.test.runTests([
          setParameterFailures,
          sendPacketFailures,
          notifyConnectionStateChangedFailures,
        ]);
      } else {
        chrome.test.assertEq(message, 'connected');
        var params = {
          address: "10.10.10.10/24",
          exclusionList: ["63.145.213.129/32", "63.145.212.0/24"],
          inclusionList: ["0.0.0.0/0", "63.145.212.128/25"],
          mtu: "1600",
          broadcastAddress: "10.10.10.255",
          domainSearch: ["foo", "bar"],
          dnsServers: ["8.8.8.8"],
          reconnect: "false"
        };
        chrome.vpnProvider.setParameters(params, onSetParameterComplete);
      }
    });
    chrome.vpnProvider.createConfig('testconfig', function() {
      chrome.test.assertEq(chrome.runtime.lastError, undefined);
      chrome.test.succeed();
    });
  },
  configInternalRemove: function() {
    chrome.vpnProvider.createConfig('testconfig', function() {
      chrome.test.assertEq(chrome.runtime.lastError, undefined);
      chrome.vpnProvider.onConfigRemoved.addListener(function(name) {
        chrome.test.assertEq(chrome.runtime.lastError, undefined);
        chrome.test.assertEq('testconfig', name);
        chrome.test.succeed();
      });
      chrome.test.succeed();
    });
  },
  destroyConnectedConfigSetup: function() {
    chrome.vpnProvider.onPlatformMessage.addListener(function(config_name,
                                                              message, error) {
      chrome.test.assertEq(message, 'disconnected');
      chrome.test.succeed();
    });
    chrome.test.succeed();
  },
  createConfigWithoutNetworkProfile: function() {
    chrome.vpnProvider.createConfig('exists', function() {
      chrome.test.assertEq(
          'No user profile for unshared network configuration.',
          chrome.runtime.lastError.message);
      chrome.test.succeed();
    });
  },
  expectEvents: function() {
    // The variable |i| is used to verify the order in which events are fired.
    var i = 0;
    chrome.vpnProvider.createConfig('testconfig', function() {
      chrome.test.assertEq(chrome.runtime.lastError, undefined);
      chrome.test.succeed();
    });
    chrome.vpnProvider.onPlatformMessage.addListener(function(config_name,
                                                              message, error) {
      chrome.test.assertEq(i, 0);
      chrome.test.assertEq(config_name, 'testconfig');
      chrome.test.assertEq(message, 'error');
      chrome.test.assertEq(error, 'error_message');
      i++;
    });
    chrome.vpnProvider.onUIEvent.addListener(function(event, id) {
      if (event == 'showAddDialog') {
        chrome.test.assertEq(i, 1);
        chrome.test.assertEq(id, '');
        i++;
      } else {
        chrome.test.assertEq(i, 2);
        chrome.test.assertEq(event, 'showConfigureDialog');
        chrome.test.assertEq(id, 'testconfig');
        chrome.test.succeed();
      }
    });
  },
  destroyConfigSuccess: function() {
    chrome.vpnProvider.destroyConfig('testconfig', function() {
      chrome.test.assertEq(chrome.runtime.lastError, undefined);
      chrome.test.succeed();
    });
  },
  platformMessage: function () {
    let i = 0;
    chrome.vpnProvider.onPlatformMessage.addListener((config_name,
                                                      message, error) => {
      chrome.test.assertEq(config_name, 'testconfig');
      if (message === 'connected') {
        chrome.test.assertEq(i, 0);
        chrome.test.succeed();
        i++;
      } else {
        chrome.test.assertEq(i, 1);
        chrome.test.assertEq(message, 'disconnected');
        chrome.test.succeed();
        i++;
      }
    });
    chrome.test.succeed();
  }
};

testRoutines[selectedTest]();

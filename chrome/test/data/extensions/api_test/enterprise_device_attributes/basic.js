// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/40697472) automate packing procedure
// Must be packed to ../enterprise_device_attributes.crx using the private key
// ../enterprise_device_attributes.pem .

chrome.test.getConfig(function(config) {
  var customArg = JSON.parse(config.customArg);
  var expectedDirectoryDeviceId = customArg.expectedDirectoryDeviceId;
  var expectedSerialNumber = customArg.expectedSerialNumber;
  var expectedAssetId = customArg.expectedAssetId;
  var expectedAnnotatedLocation = customArg.expectedAnnotatedLocation;
  var expectedHostname = customArg.expectedHostname;

  chrome.test.runTests([
    function testDirectoryDeviceId() {
      chrome.enterprise.deviceAttributes.getDirectoryDeviceId(function(
          deviceId) {
        chrome.test.assertEq(expectedDirectoryDeviceId, deviceId);
        chrome.test.succeed();
      });
    },
    function testDeviceSerialNumber() {
      chrome.enterprise.deviceAttributes.getDeviceSerialNumber(function(
          serialNumber) {
        chrome.test.assertEq(expectedSerialNumber, serialNumber);
        chrome.test.succeed();
      });

    },
    function testDeviceAssetId() {
      chrome.enterprise.deviceAttributes.getDeviceAssetId(function(assetId) {
        chrome.test.assertEq(expectedAssetId, assetId);
        chrome.test.succeed();
      });
    },
    function testDeviceAnnotatedLocation() {
      chrome.enterprise.deviceAttributes.getDeviceAnnotatedLocation(function(
          annotatedLocation) {
        chrome.test.assertEq(expectedAnnotatedLocation, annotatedLocation);
        chrome.test.succeed();
      });
    },
    function testDeviceHostname() {
      chrome.enterprise.deviceAttributes.getDeviceHostname(function(hostname) {
        chrome.test.assertEq(expectedHostname, hostname);
        chrome.test.succeed();
      });
    }
  ]);
});

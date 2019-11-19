// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testServiceEvents() {
  chrome.test.assertEq(2, Object.keys(addedServices).length);
  chrome.test.assertEq(1, Object.keys(changedServices).length);
  chrome.test.assertEq(1, Object.keys(removedServices).length);

  chrome.test.assertEq(serviceId0, addedServices[serviceId0].instanceId);
  chrome.test.assertEq(serviceId1, addedServices[serviceId1].instanceId);
  chrome.test.assertEq(serviceId1, changedServices[serviceId1].instanceId);
  chrome.test.assertEq(serviceId0, removedServices[serviceId0].instanceId);

  chrome.test.succeed();
}

var serviceId0 = 'service_id0';
var serviceId1 = 'service_id1';

var addedServices = {}
var changedServices = {}
var removedServices = {}

chrome.bluetoothLowEnergy.onServiceAdded.addListener(function (service) {
  addedServices[service.instanceId] = service;
});

chrome.bluetoothLowEnergy.onServiceChanged.addListener(function (service) {
  changedServices[service.instanceId] = service;
});

chrome.bluetoothLowEnergy.onServiceRemoved.addListener(function (service) {
  removedServices[service.instanceId] = service;
});

chrome.test.sendMessage('ready', function(message) {
  chrome.test.runTests([testServiceEvents]);
});


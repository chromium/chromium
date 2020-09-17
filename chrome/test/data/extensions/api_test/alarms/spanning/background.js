// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let inIncognito = chrome.extension.inIncognitoContext;
let alarmName = inIncognito ? 'incognito' : 'normal';

chrome.alarms.onAlarm.addListener(function(alarm) {
  chrome.test.assertEq(inIncognito ? 'incognito' : 'normal', alarm.name);
  chrome.test.succeed();
});

chrome.test.runTests([
  // Creates an alarm with the name of the context it was created in.
  function createAlarm() {
    chrome.alarms.create(alarmName, {delayInMinutes: 0.001,
                                     periodInMinutes: 60});
  },
  function getAlarm() {
    chrome.alarms.get(alarmName, function(alarm) {
      chrome.test.assertEq(alarmName, alarm.name);
      chrome.test.succeed();
    });
  },
  function getAllAlarms() {
    chrome.alarms.getAll(function(alarms) {
      chrome.test.assertEq(1, alarms.length);
      chrome.test.assertEq(alarmName, alarms[0].name);
      chrome.test.succeed();
    });
  },
  function clearAlarm() {
    chrome.alarms.clear(alarmName, function(wasCleared) {
      chrome.test.assertTrue(wasCleared);
      chrome.test.succeed();
    });
  }
]);

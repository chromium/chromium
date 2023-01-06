// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let inIncognito = chrome.extension.inIncognitoContext;
let alarmName = inIncognito ? 'incognito' : 'normal';
let createParams = {delayInMinutes: 0.001, periodInMinutes: 60};

var alarmFired = false;
var succeedOnAlarm = false;

chrome.alarms.onAlarm.addListener(function(alarm) {
  chrome.test.assertFalse(alarmFired);
  alarmFired = true;
  chrome.test.assertEq(inIncognito ? 'incognito' : 'normal', alarm.name);
  if (succeedOnAlarm) {
    chrome.test.succeed();
  }
});

chrome.test.runTests([
  // Creates an alarm with the name of the context it was created in.
  function createAlarm() {
    isWaitingForAlarm = true;
    chrome.alarms.create(alarmName, createParams, () => {
      chrome.test.assertNoLastError();
      // The alarm (which was set for an obscenely short amount of time) could
      // potentially already have fired. If so, succeed now; else, the test will
      // succeed when it fires.
      if (alarmFired) {
        chrome.test.succed();
      } else {
        succeedOnAlarm = true;
      }
    });
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
  },
  function clearAlarms() {
    chrome.alarms.create(alarmName + '-1', createParams);
    chrome.alarms.create(alarmName + '-2', createParams);
    chrome.alarms.clearAll(function(wasCleared) {
      chrome.test.assertTrue(wasCleared);
      chrome.test.succeed();
    });
  },
]);

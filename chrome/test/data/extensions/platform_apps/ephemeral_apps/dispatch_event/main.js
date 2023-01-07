// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create('index.html', {});
});

chrome.app.runtime.onRestarted.addListener(function() {
  chrome.test.sendMessage('restarted');
});

chrome.alarms.onAlarm.addListener(function(alarmInfo) {
  chrome.test.sendMessage('alarm_received');
});

chrome.runtime.onInstalled.addListener(function() {
  chrome.test.sendMessage('installed');
});

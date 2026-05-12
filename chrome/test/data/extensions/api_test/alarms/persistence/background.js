// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig((config) => {
  const phase = config.customArg;

  async function createPersistentAndSessionAlarms() {
    await chrome.alarms.create(
        'persistent_alarm', {delayInMinutes: 10, persistAcrossSessions: true});

    await chrome.alarms.create(
        'session_alarm', {delayInMinutes: 15, persistAcrossSessions: false});

    const alarms = await chrome.alarms.getAll();
    chrome.test.assertEq(2, alarms.length, 'Should have 2 alarms initially');

    const persistent = alarms.find((a) => a.name === 'persistent_alarm');
    const session = alarms.find((a) => a.name === 'session_alarm');

    chrome.test.assertTrue(!!persistent, 'persistent exists');
    chrome.test.assertTrue(!!session, 'session exists');

    chrome.test.assertEq(true, persistent.persistAcrossSessions);
    chrome.test.assertEq(false, session.persistAcrossSessions);

    chrome.test.succeed();
  }

  async function verifyAfterReloadExpectOne() {
    const alarms = await chrome.alarms.getAll();

    // After *extension reload*, the session alarm should be gone.
    chrome.test.assertEq(
        1, alarms.length, 'Only persistent alarm should remain');
    const alarm = alarms[0];
    chrome.test.assertEq('persistent_alarm', alarm.name);
    chrome.test.assertEq(true, alarm.persistAcrossSessions);
    chrome.test.succeed();
  }

  async function verifyAfterRestartExpectOne() {
    const alarms = await chrome.alarms.getAll();

    // After *browser restart*, the persistent alarm should still exist.
    chrome.test.assertEq(
        1, alarms.length, 'Only persistent alarm should remain');
    const alarm = alarms[0];
    chrome.test.assertEq('persistent_alarm', alarm.name);
    chrome.test.assertEq(true, alarm.persistAcrossSessions);
    chrome.test.succeed();
  }

  if (phase === 'create') {
    chrome.test.runTests([createPersistentAndSessionAlarms]);
  } else if (phase === 'verify_after_reload') {
    chrome.test.runTests([verifyAfterReloadExpectOne]);
  } else if (phase === 'verify_after_restart') {
    chrome.test.runTests([verifyAfterRestartExpectOne]);
  } else {
    // Fallback: fail loudly if phase not set.
    chrome.test.runTests([() => chrome.test.fail('unknown phase')]);
  }

  chrome.test.sendMessage('ready');
});

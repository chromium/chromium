// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('PasswordManagerInternals', function() {
  test('LogText', () => {
    const divLogs = document.getElementById('log-entries');
    assertTrue(!!divLogs);
    assertNotEquals(null, divLogs, 'The <div> with logs not found.');
    assertNotEquals(
        null, divLogs.innerHTML.match(/text for testing/),
        'The logged text not found.');
    assertEquals(
        null, divLogs.innerHTML.match(/<script>/),
        'The logged text was not escaped.');
  });

  test('LogEmpty', () => {
    const divLogs = document.getElementById('log-entries');
    assertTrue(!!divLogs);
    assertNotEquals(null, divLogs, 'The <div> with logs not found.');
    assertEquals(
        null, divLogs.innerHTML.match(/[^\s]/),
        'There were some logs:' + divLogs.innerHTML);
  });

  test('NonIncognitoDescription', () => {
    const body = document.getElementsByTagName('body')[0];
    assertTrue(!!body);
    const bodyText = body.innerText;
    let match = bodyText.match(/logs are listed below/);
    assertTrue(!!match);
    assertEquals(1, match.length, 'Where are the logs in: ' + bodyText);
    match = bodyText.match(/in Incognito/);
    assertEquals(null, match);
  });

  test('IncognitoDescription', () => {
    const body = document.getElementsByTagName('body')[0];
    assertTrue(!!body);
    const bodyText = body.innerText;
    let match = bodyText.match(/in Incognito/);
    assertTrue(!!match);
    assertEquals(1, match.length, 'Where is Incognito in: ' + bodyText);
    match = bodyText.match(/logs are listed below/);
    assertEquals(null, match);
  });
});

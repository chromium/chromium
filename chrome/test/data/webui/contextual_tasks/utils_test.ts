// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isGoogleOrigin} from 'chrome://contextual-tasks/utils.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('ContextualTasksUtilsTest', () => {
  test('isGoogleOrigin matches valid google origins', () => {
    assertTrue(isGoogleOrigin('https://google.com'));
    assertTrue(isGoogleOrigin('https://www.google.com'));
    assertTrue(isGoogleOrigin('https://sub.google.com'));
    assertTrue(isGoogleOrigin('https://google.co.uk'));
    assertTrue(isGoogleOrigin('https://www.google.co.uk'));
    assertTrue(isGoogleOrigin('https://sub.google.co.uk'));
    assertTrue(isGoogleOrigin('https://google.es'));
    assertTrue(isGoogleOrigin('https://www.google.es'));
  });

  test('isGoogleOrigin matches valid googlers origins', () => {
    assertTrue(isGoogleOrigin('https://googlers.com'));
    assertTrue(isGoogleOrigin('https://www.googlers.com'));
    assertTrue(isGoogleOrigin('https://sub.googlers.com'));
    assertTrue(isGoogleOrigin('https://googlers.co.uk'));
    assertTrue(isGoogleOrigin('https://www.googlers.co.uk'));
  });

  test('isGoogleOrigin rejects invalid google origins', () => {
    assertFalse(isGoogleOrigin('http://google.com'));  // http
    assertFalse(
        isGoogleOrigin('https://google.com.attacker.com'));  // subdomain hijack
    assertFalse(
        isGoogleOrigin('https://attacker-google.com'));  // domain prefix hijack
    assertFalse(isGoogleOrigin('https://google.attacker.com'));  // wrong domain
    assertFalse(
        isGoogleOrigin('https://googlers.attacker.com'));  // wrong domain
    assertFalse(isGoogleOrigin(
        'https://google.com/path'));  // origin shouldn't have path (though test
                                      // it anyway)
    assertFalse(
        isGoogleOrigin('http://localhost.attacker.com'));  // localhost hijack
  });

  test('isGoogleOrigin matches bypass origins in dev builds', () => {
    // <if expr="not is_official_build">
    // These should return true because the test is built as a non-official
    // build.
    assertTrue(isGoogleOrigin('http://localhost'));
    assertTrue(isGoogleOrigin('http://localhost:8080'));
    assertTrue(isGoogleOrigin('null'));
    assertTrue(isGoogleOrigin('file://'));
    assertTrue(isGoogleOrigin('file:///path/to/file'));
    // </if>
    // <if expr="is_official_build">
    // These should return false because the test is built as an official build.
    assertFalse(isGoogleOrigin('http://localhost'));
    assertFalse(isGoogleOrigin('http://localhost:8080'));
    assertFalse(isGoogleOrigin('null'));
    assertFalse(isGoogleOrigin('file://'));
    assertFalse(isGoogleOrigin('file:///path/to/file'));
    // </if>
  });
});

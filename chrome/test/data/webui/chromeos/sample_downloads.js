// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Sample tests that exercise the test JS library and show how this framework
// could be used to test the downloads page.
function testAssertFalse() {
  assertFalse(false);
}

function DISABLED_testAssertFalse() {
  assertFalse(true);
  assertFalse(false);
}

function testInitialFocus() {
  assertTrue(document.activeElement.id === 'term', '');
}

function testConsoleError() {
  console.error('checking console.error call causes failure.');
}

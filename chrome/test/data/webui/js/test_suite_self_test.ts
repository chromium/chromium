// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

// Test suite used to test the WebUIMochaBrowserTest C++ class itself. See
// chrome/test/base/web_ui_mocha_browser_test_browsertest.cc for usages.
suite('TestSuiteSelfTest', function() {
  test('Success', function() {
    assertTrue(true);
  });

  // This test intentionally fails, to ensure that the failure is correctly
  // detected on he C++ side.
  test('Failure', function() {
    assertTrue(false);
  });
});

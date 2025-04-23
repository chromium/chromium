// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

const MAX_LIMIT = 512;

const testName = 'a'.repeat(MAX_LIMIT + 1);

// Test suite used to test the WebUIMochaBrowserTest C++ class itself. See
// chrome/test/base/web_ui_mocha_browser_test_browsertest.cc for usages.
suite('LongTestNameTestSuiteSelfTest', function() {
  test(testName, function() {
    assertTrue(true);
  });
});

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getCommonFetchHeaderTests} from '/_test_resources/api_test/service_worker/worker_fetch_headers/fetch_common_tests.js';

chrome.test.getConfig(function(config) {
  var runBackgroundTests = config.customArg == 'run_background_tests';
  if (runBackgroundTests) {
    chrome.test.runTests(getCommonFetchHeaderTests());
  }
});

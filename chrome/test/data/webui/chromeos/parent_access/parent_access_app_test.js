// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../mojo_webui_test_support.js';
import 'chrome://parent-access/parent_access_app.js';

import {Screens} from 'chrome://parent-access/parent_access_app.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

window.parent_access_app_tests = {};
parent_access_app_tests.suiteName = 'ParentAccessAppTest';

/** @enum {string} */
parent_access_app_tests.TestNames = {
  TestShowAfterFlow: 'Tests that the after flow is shown',
};

suite(parent_access_app_tests.suiteName, function() {
  let parentAccessApp;

  setup(function() {
    PolymerTest.clearBody();
    parentAccessApp = document.createElement('parent-access-app');
    document.body.appendChild(parentAccessApp);
    flush();
  });

  test(parent_access_app_tests.TestNames.TestShowAfterFlow, function() {
    assertEquals(parentAccessApp.currentScreen_, Screens.ONLINE_FLOW);
    parentAccessApp.fire('show-after');
    assertEquals(parentAccessApp.currentScreen_, Screens.AFTER_FLOW);
  });
});

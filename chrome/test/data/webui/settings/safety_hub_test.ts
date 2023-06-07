// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {SettingsSafetyHubPageElement} from 'chrome://settings/lazy_load.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
// clang-format on

suite('SafetyHubTests', function() {
  let testElement: SettingsSafetyHubPageElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-safety-hub-page');
    document.body.appendChild(testElement);
    return flushTasks();
  });

  test('DummyTest', async function() {
    const container = testElement.shadowRoot!.querySelector('.tile-container');
    assertTrue(!!container);
  });
});

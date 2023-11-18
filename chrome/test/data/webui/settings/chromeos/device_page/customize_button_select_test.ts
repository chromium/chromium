// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';
import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {CustomizeButtonSelectElement} from 'chrome://os-settings/lazy_load.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<customize-button-select>', () => {
  let select: CustomizeButtonSelectElement;

  setup(() => {
    assert(window.trustedTypes);
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  teardown(async () => {
    if (!select) {
      return;
    }
    select.remove();
    await flushTasks();
  });

  function initializeSelect() {
    select = document.createElement(CustomizeButtonSelectElement.is);
    document.body.appendChild(select);
    return flushTasks();
  }

  test('Initialize customize button select', async () => {
    await initializeSelect();

    assertTrue(!!select);
  });
});

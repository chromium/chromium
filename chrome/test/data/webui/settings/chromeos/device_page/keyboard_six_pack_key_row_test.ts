// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {KeyboardSixPackKeyRowElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<keyboard-six-pack-key-row>', () => {
  let sixPackKeyRow: KeyboardSixPackKeyRowElement;

  setup(() => {
    assert(window.trustedTypes);
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  teardown(async () => {
    if (!sixPackKeyRow) {
      return;
    }
    sixPackKeyRow.remove();
    await flushTasks();
  });

  function initializeKeyboardSixPackKeyRow() {
    sixPackKeyRow = document.createElement(KeyboardSixPackKeyRowElement.is);
    document.body.appendChild(sixPackKeyRow);
    return flushTasks();
  }

  test('Initialize six pack key row', async () => {
    await initializeKeyboardSixPackKeyRow();
    assertTrue(!!sixPackKeyRow.shadowRoot!.querySelector('#sixPackKeyRow'));
  });
});

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';
import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {CustomizeButtonDropdownItemElement} from 'chrome://os-settings/lazy_load.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<customize-button-dropdown-item>', () => {
  let dropdownItem: CustomizeButtonDropdownItemElement;

  setup(() => {
    assert(window.trustedTypes);
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  teardown(async () => {
    if (!dropdownItem) {
      return;
    }
    dropdownItem.remove();
    await flushTasks();
  });

  function initializeDropdownItem() {
    dropdownItem =
        document.createElement(CustomizeButtonDropdownItemElement.is);
    document.body.appendChild(dropdownItem);
    return flushTasks();
  }

  test('Initialize customize button dropdown item', async () => {
    await initializeDropdownItem();

    assertTrue(!!dropdownItem);
  });
});

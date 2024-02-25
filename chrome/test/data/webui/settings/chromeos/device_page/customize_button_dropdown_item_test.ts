// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';
import 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {CustomizeButtonDropdownItemElement} from 'chrome://os-settings/lazy_load.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<customize-button-dropdown-item>', () => {
  let dropdownItem: CustomizeButtonDropdownItemElement;
  let dropdownSelectedEventCount: number = 0;

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

    dropdownItem.addEventListener(
        'customize-button-dropdown-selected', function() {
          dropdownSelectedEventCount++;
        });

    document.body.appendChild(dropdownItem);
    return flushTasks();
  }

  function getOptionElement(): HTMLElement {
    const option =
        dropdownItem.shadowRoot!.querySelector('#container')! as HTMLElement;
    assertTrue(!!option);
    return option!;
  }

  test('Initialize customize button dropdown item', async () => {
    await initializeDropdownItem();

    dropdownItem.set('option', {
      value: 'none',
      name: 'Default',
    });

    await flushTasks();
    assertTrue(!!dropdownItem);
    assertEquals('Default', getOptionElement()!.textContent?.trim());

    dropdownItem.set('option', {
      value: 'key combination',
      name: 'ctrl + z',
    });

    await flushTasks();
    assertEquals('ctrl + z', getOptionElement()!.textContent?.trim());
  });

  test('clicking option item will fire event', async () => {
    await initializeDropdownItem();

    dropdownItem.set('option', {
      value: 'open key combination dialog',
      name: 'key combination',
    });

    getOptionElement()!.click();
    await flushTasks();

    assertEquals(dropdownSelectedEventCount, 1);
  });
});

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://new-tab-page/lazy_load.js';

import {CustomizeDialogElement} from 'chrome://new-tab-page/lazy_load.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {keydown} from './test_support.js';

suite('NewTabPageCustomizeDialogFocusTest', () => {
  let customizeDialog: CustomizeDialogElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    customizeDialog = document.createElement('ntp-customize-dialog');
    document.body.appendChild(customizeDialog);
    return flushTasks();
  });

  test('space selects focused menu item', () => {
    // Arrange.
    const menuItem = customizeDialog.shadowRoot!.querySelector<HTMLElement>(
        '.menu-item[page-name=themes]');
    assertTrue(!!menuItem);
    menuItem.focus();

    // Act.
    keydown(menuItem, ' ');

    // Assert.
    const selector = customizeDialog.$.menu.querySelector('iron-selector');
    assertTrue(!!selector);
    assertEquals('themes', selector.selected);
  });

  test('enter selects focused menu item', () => {
    // Arrange.
    const menuItem = customizeDialog.shadowRoot!.querySelector<HTMLElement>(
        '.menu-item[page-name=shortcuts]');
    assertTrue(!!menuItem);
    menuItem.focus();

    // Act.
    keydown(menuItem, 'Enter');

    // Assert.
    const selector = customizeDialog.$.menu.querySelector('iron-selector');
    assertTrue(!!selector);
    assertEquals('shortcuts', selector.selected);
  });
});

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://new-tab-page/lazy_load.js';

import {flushTasks} from 'chrome://webui-test/test_util.js';

import {keydown} from './test_support.js';

suite('NewTabPageCustomizeDialogFocusTest', () => {
  /** @type {!CustomizeDialogElement} */
  let customizeDialog;

  setup(() => {
    PolymerTest.clearBody();

    customizeDialog = document.createElement('ntp-customize-dialog');
    document.body.appendChild(customizeDialog);
    return flushTasks();
  });

  test('space selects focused menu item', () => {
    // Arrange.
    const menuItem = customizeDialog.shadowRoot.querySelector(
        '.menu-item[page-name=themes]');
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
    const menuItem = customizeDialog.shadowRoot.querySelector(
        '.menu-item[page-name=shortcuts]');
    menuItem.focus();

    // Act.
    keydown(menuItem, 'Enter');

    // Assert.
    const selector = customizeDialog.$.menu.querySelector('iron-selector');
    assertTrue(!!selector);
    assertEquals('shortcuts', selector.selected);
  });
});

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PasswordManagerSideBarElement} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('PasswordManagerSideBarTest', function() {
  let sidebar: PasswordManagerSideBarElement;

  setup(function() {
    document.body.innerHTML = '';
    sidebar = document.createElement('password-manager-side-bar');
    document.body.appendChild(sidebar);
  });

  test('check layout', function() {
    assertTrue(isVisible(sidebar));
    const sideBarEntries = sidebar.shadowRoot!.querySelectorAll('a');
    assertEquals(3, sideBarEntries.length);
  });
});

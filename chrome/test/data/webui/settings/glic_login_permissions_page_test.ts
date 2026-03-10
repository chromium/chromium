// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';
import 'chrome://settings/settings.js';

import type {SettingsGlicLoginPermissionsPageElement} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('GlicLoginPermissionsPage', function() {
  let page: SettingsGlicLoginPermissionsPageElement;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-glic-login-permissions-page');
    document.body.appendChild(page);
    await flushTasks();
  });

  test('login permissions list is visible', () => {
    const loginPermissionsList =
        page.shadowRoot!.querySelector('#actorLoginPermissionsList');
    assertTrue(!!loginPermissionsList);
    assertEquals(
        0, page.shadowRoot!.querySelectorAll('.permission-item').length);
  });
});

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {SettingsSafetyHubEntryPointElement} from 'chrome://settings/lazy_load.js';
import {Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
// clang-format on

suite('SafetyHubEntryPointUI', function() {
  let page: SettingsSafetyHubEntryPointElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-safety-hub-entry-point');
    document.body.appendChild(page);
  });

  teardown(function() {
    page.remove();
  });

  test('Nothing to do state', function() {
    const element =
        page.shadowRoot!.querySelector('settings-safety-hub-module');

    assertFalse(element!.hasAttribute('header'));
    assertTrue(element!.hasAttribute('subheader'));
    assertEquals(
        element!.getAttribute('subheader')!.trim(),
        page.i18n('safetyHubEntryPointNothingToDo'));

    // Entry point has button leading to Safety Hub.
    page.$.button.click();
    assertEquals(Router.getInstance().getCurrentRoute(), routes.SAFETY_HUB);
  });
});

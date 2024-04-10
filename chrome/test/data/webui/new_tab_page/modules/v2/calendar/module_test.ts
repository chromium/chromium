// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CalendarModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {googleCalendarDescriptor} from 'chrome://new-tab-page/lazy_load.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('NewTabPageModulesCalendarModuleTest', () => {
  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('creates module', async () => {
    const module =
        await googleCalendarDescriptor.initialize(0) as CalendarModuleElement;
    assertTrue(!!module);
    document.body.append(module);
    await waitAfterNextRender(module);

    // Assert.
    assertTrue(
        isVisible(module.shadowRoot!.querySelector('ntp-module-header-v2')));
  });
});

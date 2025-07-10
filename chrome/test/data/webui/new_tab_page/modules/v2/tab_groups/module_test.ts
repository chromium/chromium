// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TabGroupsModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {tabGroupsDescriptor} from 'chrome://new-tab-page/lazy_load.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('NewTabPageModulesTabGroupsModuleTest', () => {
  let module: TabGroupsModuleElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    module = await tabGroupsDescriptor.initialize(0) as TabGroupsModuleElement;
    document.body.append(module);
    return microtasksFinished();
  });

  test('creates module', () => {
    // Assert.
    assertTrue(!!module);
    assertTrue(
        isVisible(module.shadowRoot.querySelector('ntp-module-header-v2')));
  });
});

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import type {SettingsYourSavedInfoPageIndexElement} from 'chrome://settings/settings.js';
import {loadTimeData, resetRouterForTesting, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('YourSavedInfoPageIndex', function() {
  let index: SettingsYourSavedInfoPageIndexElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // routes.YOUR_SAVED_INFO does not exist if enableYourSavedInfoSettingsPage
    // is false
    loadTimeData.overrideValues({enableYourSavedInfoSettingsPage: true});
    resetRouterForTesting();

    index = document.createElement('settings-your-saved-info-page-index');
    document.body.appendChild(index);
    return flushTasks();
  });

  test('Routing', async function() {
    function assertActiveView(id: string) {
      assertTrue(
          !!index.$.viewManager.querySelector(`#${id}.active[slot=view]`));
      assertFalse(!!index.$.viewManager.querySelector(
          `.active[slot=view]:not(#${id})`));
    }

    assertEquals(routes.BASIC, Router.getInstance().getCurrentRoute());
    assertActiveView('parent');

    Router.getInstance().navigateTo(routes.YOUR_SAVED_INFO);
    await microtasksFinished();
    assertActiveView('parent');
  });
});

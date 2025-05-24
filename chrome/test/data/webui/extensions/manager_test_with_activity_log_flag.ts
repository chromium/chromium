// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://extensions/extensions.js';

import type {ExtensionsManagerElement} from 'chrome://extensions/extensions.js';
import {navigation, Page} from 'chrome://extensions/extensions.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('ExtensionManagerTest', function() {
  let manager: ExtensionsManagerElement;

  function assertViewActive(tagName: string) {
    assertTrue(!!manager.$.viewManager.querySelector(`${tagName}.active`));
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    window.history.replaceState(
        {}, '', '/?id=ldnnhddmnhbkjipkidpdiheffobcpfmf');

    manager = document.createElement('extensions-manager');
    document.body.appendChild(manager);

    // Wait for the first view to be active before starting tests, if one is
    // not active already. Sometimes, on Mac with native HTML imports
    // disabled, no views are active at this point.
    return manager.$.viewManager.querySelector('.active') ?
        Promise.resolve() :
        eventToPromise('view-enter-start', manager);
  });

  test('UrlNavigationToActivityLogSuccess', async () => {
    assertTrue(manager.showActivityLog);

    // Try to open activity log with a valid ID.
    navigation.navigateTo({
      page: Page.ACTIVITY_LOG,
      extensionId: 'ldnnhddmnhbkjipkidpdiheffobcpfmf',
    });
    await microtasksFinished();

    // Should be on activity log page.
    assertViewActive('extensions-activity-log');

    // Try to open activity log with an invalid ID.
    navigation.navigateTo(
        {page: Page.ACTIVITY_LOG, extensionId: 'z'.repeat(32)});
    await microtasksFinished();
    // Should also be on activity log page. See |changePage_| in manager.js
    // for the use case.
    assertViewActive('extensions-activity-log');
  });
});

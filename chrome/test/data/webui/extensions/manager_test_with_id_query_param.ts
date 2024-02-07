// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://extensions/extensions.js';

import type {ExtensionsManagerElement} from 'chrome://extensions/extensions.js';
import {navigation, Page} from 'chrome://extensions/extensions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

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

  test('UrlNavigationToDetails', function() {
    assertViewActive('extensions-detail-view');
    const detailsView =
        manager.shadowRoot!.querySelector('extensions-detail-view');
    assertTrue(!!detailsView);
    assertEquals('ldnnhddmnhbkjipkidpdiheffobcpfmf', detailsView.data.id);

    // Try to open detail view for invalid ID.
    navigation.navigateTo({page: Page.DETAILS, extensionId: 'z'.repeat(32)});
    flush();
    // Should be re-routed to the main page.
    assertViewActive('extensions-item-list');

    // Try to open detail view with a valid ID.
    navigation.navigateTo({
      page: Page.DETAILS,
      extensionId: 'ldnnhddmnhbkjipkidpdiheffobcpfmf',
    });
    flush();
    assertViewActive('extensions-detail-view');
  });

  test('UrlNavigationToActivityLogFail', function() {
    assertFalse(manager.showActivityLog);

    // Try to open activity log with a valid ID.
    navigation.navigateTo({
      page: Page.ACTIVITY_LOG,
      extensionId: 'ldnnhddmnhbkjipkidpdiheffobcpfmf',
    });
    flush();

    // Should be re-routed to details page with showActivityLog set to
    // false.
    assertViewActive('extensions-detail-view');
    const detailsView =
        manager.shadowRoot!.querySelector('extensions-detail-view');
    assertTrue(!!detailsView);
    assertFalse(detailsView.showActivityLog);

    // Try to open activity log with an invalid ID.
    navigation.navigateTo(
        {page: Page.ACTIVITY_LOG, extensionId: 'z'.repeat(32)});
    flush();
    // Should be re-routed to the main page.
    assertViewActive('extensions-item-list');
  });
});

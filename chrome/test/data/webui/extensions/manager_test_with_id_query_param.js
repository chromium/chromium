// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {navigation, Page} from 'chrome://extensions/extensions.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {eventToPromise} from '../test_util.m.js';

window.extension_manager_tests = {};
extension_manager_tests.suiteName = 'ExtensionManagerTest';
/** @enum {string} */
extension_manager_tests.TestNames = {
  UrlNavigationToDetails: 'url navigation to details',
  UrlNavigationToActivityLogFail:
      'url navigation to activity log without flag set',
};

function getDataByName(list, name) {
  return assert(list.find(function(el) {
    return el.name == name;
  }));
}

suite(extension_manager_tests.suiteName, function() {
  /** @type {Manager} */
  let manager;

  /** @param {string} viewElement */
  function assertViewActive(tagName) {
    assertTrue(!!manager.$.viewManager.querySelector(`${tagName}.active`));
  }

  setup(function() {
    PolymerTest.clearBody();
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

  test(
      assert(extension_manager_tests.TestNames.UrlNavigationToDetails),
      function() {
        assertViewActive('extensions-detail-view');
        const detailsView = manager.$$('extensions-detail-view');
        expectEquals('ldnnhddmnhbkjipkidpdiheffobcpfmf', detailsView.data.id);

        // Try to open detail view for invalid ID.
        navigation.navigateTo(
            {page: Page.DETAILS, extensionId: 'z'.repeat(32)});
        flush();
        // Should be re-routed to the main page.
        assertViewActive('extensions-item-list');

        // Try to open detail view with a valid ID.
        navigation.navigateTo({
          page: Page.DETAILS,
          extensionId: 'ldnnhddmnhbkjipkidpdiheffobcpfmf'
        });
        flush();
        assertViewActive('extensions-detail-view');
      });

  test(
      assert(extension_manager_tests.TestNames.UrlNavigationToActivityLogFail),
      function() {
        expectFalse(manager.showActivityLog);

        // Try to open activity log with a valid ID.
        navigation.navigateTo({
          page: Page.ACTIVITY_LOG,
          extensionId: 'ldnnhddmnhbkjipkidpdiheffobcpfmf'
        });
        flush();

        // Should be re-routed to details page with showActivityLog set to
        // false.
        assertViewActive('extensions-detail-view');
        const detailsView = manager.$$('extensions-detail-view');
        expectFalse(detailsView.showActivityLog);

        // Try to open activity log with an invalid ID.
        navigation.navigateTo(
            {page: Page.ACTIVITY_LOG, extensionId: 'z'.repeat(32)});
        flush();
        // Should be re-routed to the main page.
        assertViewActive('extensions-item-list');
      });
});

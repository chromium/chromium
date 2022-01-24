// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {navigation, Page} from 'chrome://extensions/extensions.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {eventToPromise} from '../test_util.js';


window.extension_manager_tests = {};
extension_manager_tests.suiteName = 'ExtensionManagerTest';
/** @enum {string} */
extension_manager_tests.TestNames = {
  UrlNavigationToSiteAccessSuccess:
      'url navigation to site access page with flag set',
};

suite(extension_manager_tests.suiteName, function() {
  /** @type {ExtensionsManagerElement} */
  let manager;

  /** @param {string} tagName */
  function assertViewActive(tagName) {
    assertTrue(!!manager.$.viewManager.querySelector(`${tagName}.active`));
  }

  setup(function() {
    document.body.innerHTML = '';
    window.history.replaceState(
        {}, '', '/?id=ldnnhddmnhbkjipkidpdiheffobcpfmf');

    manager = document.createElement('extensions-manager');
    document.body.appendChild(manager);

    // Wait for the first view to be active before starting tests.
    return eventToPromise('view-enter-start', manager);
  });


  test(
      assert(
          extension_manager_tests.TestNames.UrlNavigationToSiteAccessSuccess),
      function() {
        expectTrue(manager.useNewSiteAccessPage);

        // Try to open the extensions site access page with a valid ID.
        navigation.navigateTo({
          page: Page.EXTENSION_SITE_ACCESS,
          extensionId: 'ldnnhddmnhbkjipkidpdiheffobcpfmf'
        });

        flush();
        assertViewActive('extensions-site-access');

        // Try to open extensions site access page with an invalid ID.
        navigation.navigateTo(
            {page: Page.EXTENSION_SITE_ACCESS, extensionId: 'z'.repeat(32)});
        flush();
        // Should be redirected to the list page.
        assertViewActive('extensions-item-list');
      });
});

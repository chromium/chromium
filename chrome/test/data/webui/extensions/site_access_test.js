// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {navigation, Page} from 'chrome://extensions/extensions.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {createExtensionInfo} from './test_util.js';

/** @fileoverview Suite of tests for extensions-site-access. */
suite('ExtensionsSiteAccessTest', function() {
  /**
   * Backing extension id, same id as the one in
   * createExtensionInfo
   * @type {string}
   */
  const EXTENSION_ID = 'a'.repeat(32);

  /**
   * Extension site access element created before each test.
   * @type {ExtensionsSiteAccess}
   */
  let extensionsSiteAccess;

  /**
   * Backing extension info for the site access page.
   * @type {chrome.developerPrivate.ExtensionInfo}
   */
  let extensionInfo;

  /**
   * ID of a navigation listener. Cleared after every test.
   * @type {Number}
   */
  let listenerId = 0;

  // Initialize an extension site access element before each test.
  setup(function() {
    document.body.innerHTML = '';

    extensionsSiteAccess = document.createElement('extensions-site-access');

    extensionInfo = createExtensionInfo({
      id: EXTENSION_ID,
    });
    extensionsSiteAccess.extensionInfo = extensionInfo;

    document.body.appendChild(extensionsSiteAccess);

    extensionsSiteAccess.dispatchEvent(
        new CustomEvent('view-enter-start', {bubbles: true}));
  });

  teardown(function() {
    navigation.removeListener(listenerId);
  });

  test('clicking on back button navigates to the details page', function() {
    flush();

    let currentPage = null;
    listenerId = navigation.addListener(newPage => {
      currentPage = newPage;
    });

    extensionsSiteAccess.shadowRoot.querySelector('#closeButton').click();
    expectDeepEquals(
        currentPage, {page: Page.DETAILS, extensionId: EXTENSION_ID});
  });
});

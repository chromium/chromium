// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-site-permissions-all-sites. */
import 'chrome://extensions/extensions.js';

import {ExtensionsSitePermissionsBySiteElement, navigation, Page} from 'chrome://extensions/extensions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('SitePermissionsBySite', function() {
  let element: ExtensionsSitePermissionsBySiteElement;
  let listenerId: number = 0;

  setup(function() {
    document.body.innerHTML = '';
    element = document.createElement('extensions-site-permissions-by-site');
    document.body.appendChild(element);
  });

  teardown(function() {
    if (listenerId !== 0) {
      assertTrue(navigation.removeListener(listenerId));
      listenerId = 0;
    }
  });

  test(
      'clicking close button navigates back to site permissions page',
      function() {
        let currentPage = null;
        listenerId = navigation.addListener(newPage => {
          currentPage = newPage;
        });

        flush();
        const closeButton = element.$.closeButton;
        assertTrue(!!closeButton);
        assertTrue(isVisible(closeButton));

        closeButton.click();
        flush();

        assertDeepEquals(currentPage, {page: Page.SITE_PERMISSIONS});
      });
});

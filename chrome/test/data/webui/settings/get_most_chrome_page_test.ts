// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {IronCollapseElement, SettingsGetMostChromePageElement} from 'chrome://settings/lazy_load.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

/** @fileoverview Suite of tests for get_most_chrome_page. */
suite('GetMostChromePage', function() {
  let testElement: SettingsGetMostChromePageElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-get-most-chrome-page');
    document.body.appendChild(testElement);
    flush();
  });

  test('Basic', function() {
    const rows = testElement.shadowRoot!.querySelectorAll('cr-expand-button');
    assertTrue(rows.length > 0);
    rows.forEach((row) => {
      const ironCollapse = row.nextElementSibling as IronCollapseElement;
      assertTrue(!!ironCollapse);

      assertFalse(ironCollapse.opened);
      row.click();
      assertTrue(ironCollapse.opened);
    });
  });
});

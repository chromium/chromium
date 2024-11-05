// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for the Security Keys settings subpage.
 */

import 'chrome://settings/lazy_load.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SecurityKeysSubpageElement} from 'chrome://settings/lazy_load.js';
import {resetRouterForTesting, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('SecurityKeysSubpage', function() {
  let page: SecurityKeysSubpageElement;

  async function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('security-keys-subpage');
    document.body.appendChild(page);
    await flushTasks();
  }

  // TODO(crbug.com/372493822): remove these tests when hybrid linking flag is
  // removed.
  test(
      'Shows the manage phones button if hybrid linking is enabled',
      async function() {
        loadTimeData.overrideValues({enableSecurityKeysManagePhones: true});
        await createPage();
        resetRouterForTesting();
        const button =
            page.shadowRoot!.querySelector<HTMLElement>('#managePhonesButton');
        assertTrue(!!button);
        button.click();
        flush();
        assertEquals(
            routes.SECURITY_KEYS_PHONES,
            Router.getInstance().getCurrentRoute());
      });

  test(
      'Does not show the manage phones button if hybrid linking is disabled',
      async function() {
        loadTimeData.overrideValues({enableSecurityKeysManagePhones: false});
        await createPage();
        resetRouterForTesting();
        const button =
            page.shadowRoot!.querySelector<HTMLElement>('#managePhonesButton');
        assertFalse(!!button);
      });
});

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_proxy_exclusions.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('NetworkProxyExclusionsTest', function() {
  /** @type {!NetworkProxyExclusions|undefined} */
  let proxyExclusions;

  setup(function() {
    proxyExclusions = document.createElement('network-proxy-exclusions');
    document.body.appendChild(proxyExclusions);
    flush();
  });

  test('Clear fires proxy-exclusions-change event', function(done) {
    proxyExclusions.exclusions = [
      'one',
      'two',
      'three',
    ];
    flush();

    // Verify that clicking the clear button fires the proxy-exclusions-change
    // event and that the item was removed from the exclusions list.
    proxyExclusions.addEventListener('proxy-exclusions-change', function() {
      assertDeepEquals(proxyExclusions.exclusions, ['two', 'three']);
      done();
    });

    const clearBtn = proxyExclusions.$$('cr-icon-button');
    assertTrue(!!clearBtn);

    // Simulate pressing clear on the first exclusion list item.
    clearBtn.click();
  });
});

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #clang-format off
import 'chrome://settings/settings.js';

import {getTrustedHTML} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
// #clang-format on

suite('Settings idle load tests', function() {
  setup(function() {
    document.body.innerHTML = getTrustedHTML`
      <settings-idle-load>
        <template>
          <div></div>
        </template>
      </settings-idle-load>
    `;
    // The div should not be initially accessible.
    assertFalse(!!document.body.querySelector('div'));
  });

  test('stamps after get()', function() {
    // Calling get() will force stamping without waiting for idle time.
    return document.body.querySelector('settings-idle-load')!.get().then(
        function(inner) {
          assertEquals('DIV', inner.nodeName);
          assertEquals(inner, document.body.querySelector('div'));
        });
  });

  test('stamps after idle', function(done) {
    requestIdleCallback(function() {
      // After JS calls idle-callbacks, this should be accessible.
      assertTrue(!!document.body.querySelector('div'));
      done();
    });
  });
});

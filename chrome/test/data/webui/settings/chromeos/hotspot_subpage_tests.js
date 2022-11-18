// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('HotspotSubpageTest', function() {
  /** @type {HotspotSubpageElement} */
  let hotspotSubpage = null;

  setup(function() {
    // Disable animations so sub-pages open within one event loop.
    testing.Test.disableAnimationsAndTransitions();
    PolymerTest.clearBody();
    hotspotSubpage = document.createElement('settings-hotspot-subpage');
    document.body.appendChild(hotspotSubpage);
    Router.getInstance().navigateTo(routes.HOTSPOT_DETAIL);

    return flushTasks();
  });

  teardown(function() {
    return flushTasks().then(() => {
      hotspotSubpage.remove();
      hotspotSubpage = null;
      Router.getInstance().resetRouteForTesting();
    });
  });

  test('Base Test', async function() {
    assertTrue(!!hotspotSubpage);
  });
});
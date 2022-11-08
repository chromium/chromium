// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('ApnSubpageTest', function() {
  /** @type {ApnSubpageElement} */
  let apnSubpage = null;

  setup(function() {
    // Disable animations so sub-pages open within one event loop.
    testing.Test.disableAnimationsAndTransitions();
    PolymerTest.clearBody();
    apnSubpage = document.createElement('apn-subpage');
    document.body.appendChild(apnSubpage);
    Router.getInstance().navigateTo(routes.APN);

    return flushTasks();
  });

  teardown(function() {
    return flushTasks().then(() => {
      apnSubpage.remove();
      apnSubpage = null;
      Router.getInstance().resetRouteForTesting();
    });
  });

  test('Check if APN list exists', async function() {
    assertTrue(!!apnSubpage);
    assertTrue(!!apnSubpage.shadowRoot.querySelector('apn-list'));
  });
});

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(jimmyxgong): Use es6 module for mojo binding (crbug/1004256).
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://diagnostics/overview_card.js';

import {fakeSystemInfo} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {getSystemDataProvider, setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

suite('OverviewCardTest', () => {
  /** @type {?HTMLElement} */
  let overviewElement = null;

  /** @type {?FakeSystemDataProvider} */
  let provider = null;

  suiteSetup(() => {
    provider = new FakeSystemDataProvider();
    setSystemDataProviderForTesting(provider);
  });

  setup(() => {
    PolymerTest.clearBody();
  });

  teardown(() => {
    overviewElement.remove();
    overviewElement = null;
    provider.reset();
  });

  /** @param {!SystemInfo} */
  function initializeOverviewCard(fakeSystemInfo) {
    assertFalse(!!overviewElement);

    // Initialize the fake data.
    provider.setFakeSystemInfo(fakeSystemInfo);

    // Add the overview card to the DOM.
    overviewElement = document.createElement('overview-card');
    assertTrue(!!overviewElement);
    document.body.appendChild(overviewElement);

    return flushTasks();
  }

  test('OverviewCardPopulated', () => {
    return initializeOverviewCard(fakeSystemInfo).then(() => {
      assertEquals(
          fakeSystemInfo.board_name,
          overviewElement.$$('#boardName').textContent);
      assertEquals(
          fakeSystemInfo.cpu_model_name,
          overviewElement.$$('#cpuModelName').textContent);
      assertEquals(
          fakeSystemInfo.total_memory_kib.toString(),
          overviewElement.$$('#totalMemory').textContent);
      assertEquals(
          fakeSystemInfo.version.milestone_version,
          overviewElement.$$('#version').textContent);
    });
  });
});
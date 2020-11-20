// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/overview_card.js';

import {SystemInfo} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeSystemInfo} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {getSystemDataProvider, setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function overviewCardTestSuite() {
  /** @type {?OverviewCardElement} */
  let overviewElement = null;

  /** @type {?FakeSystemDataProvider} */
  let provider = null;

  suiteSetup(() => {
    provider = new FakeSystemDataProvider();
    setSystemDataProviderForTesting(provider);
  });

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    overviewElement.remove();
    overviewElement = null;
    provider.reset();
  });

  /** @param {!SystemInfo} fakeSystemInfo */
  function initializeOverviewCard(fakeSystemInfo) {
    assertFalse(!!overviewElement);

    // Initialize the fake data.
    provider.setFakeSystemInfo(fakeSystemInfo);

    // Add the overview card to the DOM.
    overviewElement = /** @type {!OverviewCardElement} */ (
        document.createElement('overview-card'));
    assertTrue(!!overviewElement);
    document.body.appendChild(overviewElement);

    return flushTasks();
  }

  test('OverviewCardPopulated', () => {
    return initializeOverviewCard(fakeSystemInfo).then(() => {
      dx_utils.assertElementContainsText(
          overviewElement.$$('#marketingName'), fakeSystemInfo.marketingName);
      dx_utils.assertElementContainsText(
          overviewElement.$$('#deviceInfo'), fakeSystemInfo.boardName);
      dx_utils.assertElementContainsText(
          overviewElement.$$('#deviceInfo'),
          fakeSystemInfo.versionInfo.milestoneVersion);
    });
  });
}

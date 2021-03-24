// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/overview_card.js';

import {SystemInfo} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeSystemInfo, fakeSystemInfoWithoutBoardName, fakeSystemInfoWithTBD} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {getSystemDataProvider, setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.m.js';

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

  test('TBDMarketingNameHidden', () => {
    return initializeOverviewCard(fakeSystemInfoWithTBD).then(() => {
      assertFalse(isVisible(
          /** @type {!HTMLElement} */ (overviewElement.$$('#marketingName'))));

      // Device info should not be surrounded by parentheses when the marketing
      // name is hidden.
      const deviceInfoText = overviewElement.$$('#deviceInfo').textContent;
      assertTrue(deviceInfoText[0] !== '(');
      assertTrue(deviceInfoText[deviceInfoText.length - 1] !== ')');
    });
  });

  test('BoardNameMissing', () => {
    return initializeOverviewCard(fakeSystemInfoWithoutBoardName).then(() => {
      const versionInfo = loadTimeData.getStringF(
          'versionInfo',
          fakeSystemInfoWithoutBoardName.versionInfo.fullVersionString);
      assertEquals(
          overviewElement.$$('#deviceInfo').textContent,
          versionInfo[0].toUpperCase() + versionInfo.slice(1));
    });
  });
}

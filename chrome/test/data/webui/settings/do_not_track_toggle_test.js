// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from 'chrome://settings/settings.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

// clang-format on

suite('CrSettingsDoNotTrackToggleTest', function() {
  /** @type {TestMetricsBrowserProxy} */
  let testMetricsBrowserProxy;

  /** @type {SettingsDoNotTrackToggleElement} */
  let testElement;

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.instance_ = testMetricsBrowserProxy;
    document.body.innerHTML = '';
    testElement = /** @type {SettingsDoNotTrackToggleElement} */ (
        document.createElement('settings-do-not-track-toggle'));
    testElement.prefs = {
      enable_do_not_track: {value: false},
    };
    document.body.appendChild(testElement);
    flush();
  });

  teardown(function() {
    testElement.remove();
  });

  test('logDoNotTrackClick', async function() {
    testElement.$.toggle.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.DO_NOT_TRACK, result);
  });

  test('DialogAndToggleBehavior', function() {
    testElement.$.toggle.click();
    flush();
    assertTrue(testElement.$.toggle.checked);

    testElement.$$('.cancel-button').click();
    assertFalse(testElement.$.toggle.checked);
    assertFalse(testElement.prefs.enable_do_not_track.value);

    testElement.$.toggle.click();
    flush();
    assertTrue(testElement.$.toggle.checked);
    testElement.$$('.action-button').click();
    assertTrue(testElement.$.toggle.checked);
    assertTrue(testElement.prefs.enable_do_not_track.value);
  });
});

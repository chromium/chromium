// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SettingsDoNotTrackToggleElement} from 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from 'chrome://settings/settings.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

// clang-format on

suite('CrSettingsDoNotTrackToggleTest', function() {
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let testElement: SettingsDoNotTrackToggleElement;

  setup(function() {
    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-do-not-track-toggle');
    testElement.prefs = {
      enable_do_not_track: {
        key: 'enable_do_not_track',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
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

    testElement.shadowRoot!.querySelector<HTMLElement>(
                               '.cancel-button')!.click();
    assertFalse(testElement.$.toggle.checked);
    assertFalse(testElement.prefs.enable_do_not_track.value);

    testElement.$.toggle.click();
    flush();
    assertTrue(testElement.$.toggle.checked);
    testElement.shadowRoot!.querySelector<HTMLElement>(
                               '.action-button')!.click();
    assertTrue(testElement.$.toggle.checked);
    assertTrue(testElement.prefs.enable_do_not_track.value);
  });
});

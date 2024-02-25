// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsDoNotTrackToggleElement} from 'chrome://settings/lazy_load.js';
import type {SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

// clang-format on

suite('CrSettingsDoNotTrackToggleTest', function() {
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let testElement: SettingsDoNotTrackToggleElement;

  function toggle(): SettingsToggleButtonElement {
    return testElement.shadowRoot!.querySelector('#toggle')!;
  }

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
    toggle().click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.DO_NOT_TRACK, result);
  });

  test('DialogAndToggleBehavior', function() {
    toggle().click();
    flush();
    assertTrue(toggle().checked);

    assertEquals(
        testElement.shadowRoot!.querySelector<HTMLAnchorElement>(
                                   'a[href]')!.getAttribute('aria-description'),
        loadTimeData.getString('opensInNewTab'));
    testElement.shadowRoot!.querySelector<HTMLElement>(
                               '.cancel-button')!.click();
    assertFalse(toggle().checked);
    assertFalse(testElement.prefs.enable_do_not_track.value);

    toggle().click();
    flush();
    assertTrue(toggle().checked);
    testElement.shadowRoot!.querySelector<HTMLElement>(
                               '.action-button')!.click();
    assertTrue(toggle().checked);
    assertTrue(testElement.prefs.enable_do_not_track.value);
  });
});

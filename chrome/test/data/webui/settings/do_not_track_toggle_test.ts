// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {SettingsDoNotTrackToggleElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData, MetricsBrowserProxyImpl, PrefService, PrefsBrowserProxy, PrivacyElementInteractions} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

// clang-format on

suite('CrSettingsDoNotTrackToggleTest', function() {
  let testMetricsBrowserProxy: TestMetricsBrowserProxy;
  let testElement: SettingsDoNotTrackToggleElement;
  let prefService: PrefService;

  setup(async function() {
    const prefsBrowserProxy = new TestPrefsBrowserProxy([{
      key: 'enable_do_not_track',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    }]);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    testMetricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(testMetricsBrowserProxy);
    await createToggle();
  });

  function createToggle() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-do-not-track-toggle');
    document.body.appendChild(testElement);
  }

  teardown(function() {
    testElement.remove();
  });

  test('logDoNotTrackClick', async function() {
    testElement.$.toggle.click();
    const result =
        await testMetricsBrowserProxy.whenCalled('recordSettingsPageHistogram');
    assertEquals(PrivacyElementInteractions.DO_NOT_TRACK, result);
  });

  test('DialogAndToggleBehavior', async function() {
    testElement.$.toggle.click();
    await microtasksFinished();
    assertTrue(testElement.$.toggle.checked);

    const anchor =
        testElement.shadowRoot.querySelector<HTMLAnchorElement>('a[href]');
    assertTrue(!!anchor);
    assertEquals(
        loadTimeData.getString('opensInNewTab'),
        anchor.getAttribute('aria-description'));

    const cancelButton =
        testElement.shadowRoot.querySelector<HTMLElement>('.cancel-button');
    assertTrue(!!cancelButton);
    cancelButton.click();
    await microtasksFinished();
    assertFalse(testElement.$.toggle.checked);
    assertFalse(prefService.getPref<boolean>('enable_do_not_track').value);

    testElement.$.toggle.click();
    await microtasksFinished();
    assertTrue(testElement.$.toggle.checked);

    const actionButton =
        testElement.shadowRoot.querySelector<HTMLElement>('.action-button');
    assertTrue(!!actionButton);
    actionButton.click();
    await microtasksFinished();
    assertTrue(testElement.$.toggle.checked);
    assertTrue(prefService.getPref<boolean>('enable_do_not_track').value);
  });

  test('sublabelWithAndWithoutUniversalOptOut', function() {
    assertEquals(
        loadTimeData.getString('trackingProtectionDoNotTrackToggleSubLabel'),
        testElement.$.toggle.subLabel);

    loadTimeData.overrideValues({showUniversalOptOutSettings: true});
    createToggle();

    assertEquals(
        loadTimeData.getString(
            'trackingProtectionDoNotTrackDisclaimerToggleSubLabel'),
        testElement.$.toggle.subLabel);
  });
});

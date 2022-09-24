// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SettingsPerformancePageElement, TabDiscardExceptionEntryElement, TabDiscardExceptionListElement} from 'chrome://settings/lazy_load.js';
import {OpenWindowProxyImpl, PerformanceBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestOpenWindowProxy} from './test_open_window_proxy.js';
import {TestPerformanceBrowserProxy} from './test_performance_browser_proxy.js';
// clang-format on

suite('PerformancePage', function() {
  let performancePage: SettingsPerformancePageElement;
  let performanceBrowserProxy: TestPerformanceBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;
  let tabDiscardExceptionsList: TabDiscardExceptionListElement;

  const highEfficiencyModeEnabledPref =
      'prefs.performance_tuning.high_efficiency_mode.enabled.value';
  const tabDiscardExceptionsPref =
      'prefs.performance_tuning.tab_discarding.exceptions.value';

  function getExceptionListEntries():
      NodeListOf<TabDiscardExceptionEntryElement> {
    return tabDiscardExceptionsList.$.container
        .querySelectorAll<TabDiscardExceptionEntryElement>(
            'tab-discard-exception-entry:not([hidden])');
  }

  function clickDeleteMenuItem() {
    const buttons = tabDiscardExceptionsList.$.menu.querySelectorAll('button');
    assertEquals(2, buttons.length);
    buttons[1]!.click();
  }

  setup(function() {
    performanceBrowserProxy = new TestPerformanceBrowserProxy();
    PerformanceBrowserProxyImpl.setInstance(performanceBrowserProxy);

    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    document.body.innerHTML = '';
    performancePage = document.createElement('settings-performance-page');
    performancePage.set('prefs', {
      performance_tuning: {
        high_efficiency_mode: {
          enabled: {
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
        },
        tab_discarding: {
          exceptions: {
            type: chrome.settingsPrivate.PrefType.LIST,
            value: [],
          },
        },
      },
    });
    document.body.appendChild(performancePage);
    flush();

    tabDiscardExceptionsList = performancePage.$.tabDiscardExceptionsList;
  });

  test('testHighEfficiencyModeEnabled', function() {
    performancePage.set(highEfficiencyModeEnabledPref, true);
    assertTrue(
        performancePage.$.toggleButton.checked,
        'toggle should be checked when pref is true');
  });

  test('testHighEfficiencyModeDisabled', function() {
    performancePage.set(highEfficiencyModeEnabledPref, false);
    assertFalse(
        performancePage.$.toggleButton.checked,
        'toggle should not be checked when pref is false');
  });

  test('testLearnMoreLink', async function() {
    const learnMoreLink =
        performancePage.$.toggleButton.shadowRoot!.querySelector<HTMLElement>(
            '#highEfficiencyLearnMore');
    assertTrue(!!learnMoreLink);
    learnMoreLink.click();
    const url = await openWindowProxy.whenCalled('openURL');
    assertEquals(loadTimeData.getString('highEfficiencyLearnMoreUrl'), url);
  });

  test('testSendFeedbackLink', async function() {
    const sendFeedbackLink =
        performancePage.$.toggleButton.shadowRoot!.querySelector<HTMLElement>(
            '#highEfficiencySendFeedback');

    // <if expr="_google_chrome">
    assertTrue(!!sendFeedbackLink);
    sendFeedbackLink.click();
    await performanceBrowserProxy.whenCalled(
        'openHighEfficiencyFeedbackDialog');
    // </if>

    // <if expr="not _google_chrome">
    assertFalse(!!sendFeedbackLink);
    // </if>
  });

  test('testTabDiscardExceptionsList', function() {
    // no sites added message should be shown when list is empty
    assertFalse(tabDiscardExceptionsList.$.noSitesAdded.hidden);
    assertEquals(0, getExceptionListEntries().length);

    // list should be updated when pref is changed
    performancePage.set(tabDiscardExceptionsPref, ['foo', 'bar']);
    flush();
    assertTrue(tabDiscardExceptionsList.$.noSitesAdded.hidden);
    assertEquals(2, getExceptionListEntries().length);
  });

  test('testTabDiscardExceptionsListDelete', function() {
    performancePage.set(tabDiscardExceptionsPref, ['foo', 'bar']);
    flush();
    let entries = getExceptionListEntries();
    assertEquals(2, entries.length);
    assertEquals('foo', entries[0]!.get('site'));
    assertEquals('bar', entries[1]!.get('site'));

    entries[0]!.$.button.click();
    clickDeleteMenuItem();
    flush();
    entries = getExceptionListEntries();
    assertEquals(1, entries.length);
    assertEquals('bar', entries[0]!.get('site'));

    entries[0]!.$.button.click();
    clickDeleteMenuItem();
    flush();
    entries = getExceptionListEntries();
    assertEquals(0, entries.length);
  });
});
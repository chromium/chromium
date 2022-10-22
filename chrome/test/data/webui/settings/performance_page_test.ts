// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {HIGH_EFFICIENCY_MODE_PREF, HighEfficiencyModeExceptionListAction, OpenWindowProxyImpl, PerformanceBrowserProxyImpl, PerformanceMetricsProxyImpl, SettingsPerformancePageElement, SUBMIT_EVENT, TAB_DISCARD_EXCEPTIONS_PREF, TabDiscardExceptionDialogElement, TabDiscardExceptionEntryElement, TabDiscardExceptionListElement} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestOpenWindowProxy} from './test_open_window_proxy.js';
import {TestPerformanceBrowserProxy} from './test_performance_browser_proxy.js';
import {TestPerformanceMetricsProxy} from './test_performance_metrics_proxy.js';

suite('PerformancePage', function() {
  let performancePage: SettingsPerformancePageElement;
  let performanceBrowserProxy: TestPerformanceBrowserProxy;
  let performanceMetricsProxy: TestPerformanceMetricsProxy;
  let openWindowProxy: TestOpenWindowProxy;
  let tabDiscardExceptionsList: TabDiscardExceptionListElement;

  function getExceptionListEntries():
      NodeListOf<TabDiscardExceptionEntryElement> {
    return tabDiscardExceptionsList.$.list.querySelectorAll(
        `${TabDiscardExceptionEntryElement.is}:not([hidden])`);
  }

  function clickDeleteMenuItem() {
    const button: HTMLButtonElement|null =
        tabDiscardExceptionsList.$.menu.get().querySelector('button#delete');
    assertTrue(!!button);
    button.click();
  }

  function clickEditMenuItem() {
    const button: HTMLButtonElement|null =
        tabDiscardExceptionsList.$.menu.get().querySelector('button#edit');
    assertTrue(!!button);
    button.click();
  }

  function getDialog(): TabDiscardExceptionDialogElement|null {
    return tabDiscardExceptionsList.shadowRoot!.querySelector(
        TabDiscardExceptionDialogElement.is);
  }

  setup(function() {
    performanceBrowserProxy = new TestPerformanceBrowserProxy();
    PerformanceBrowserProxyImpl.setInstance(performanceBrowserProxy);

    performanceMetricsProxy = new TestPerformanceMetricsProxy();
    PerformanceMetricsProxyImpl.setInstance(performanceMetricsProxy);

    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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
    performancePage.setPrefValue(HIGH_EFFICIENCY_MODE_PREF, true);
    assertTrue(
        performancePage.$.toggleButton.checked,
        'toggle should be checked when pref is true');
  });

  test('testHighEfficiencyModeDisabled', function() {
    performancePage.setPrefValue(HIGH_EFFICIENCY_MODE_PREF, false);
    assertFalse(
        performancePage.$.toggleButton.checked,
        'toggle should not be checked when pref is false');
  });

  test('testHighEfficiencyModeMetrics', async function() {
    performancePage.setPrefValue(HIGH_EFFICIENCY_MODE_PREF, false);

    performancePage.$.toggleButton.click();
    let enabled = await performanceMetricsProxy.whenCalled(
        'recordHighEfficiencyModeChanged');
    assertTrue(enabled);

    performanceMetricsProxy.reset();
    performancePage.$.toggleButton.click();
    enabled = await performanceMetricsProxy.whenCalled(
        'recordHighEfficiencyModeChanged');
    assertFalse(enabled);
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

  function setupExceptionListEntries(existingRules: string[]) {
    performancePage.setPrefValue(TAB_DISCARD_EXCEPTIONS_PREF, existingRules);
    flush();
    assertDeepEquals(existingRules, tabDiscardExceptionsList.$.list.items);
  }

  test('testTabDiscardExceptionsList', function() {
    // no sites added message should be shown when list is empty
    assertFalse(tabDiscardExceptionsList.$.noSitesAdded.hidden);
    assertDeepEquals([], tabDiscardExceptionsList.$.list.items);

    // list should be updated when pref is changed
    setupExceptionListEntries(['foo', 'bar']);
    assertTrue(tabDiscardExceptionsList.$.noSitesAdded.hidden);
  });

  test('testTabDiscardExceptionsListDelete', async function() {
    setupExceptionListEntries(['foo', 'bar']);

    getExceptionListEntries()[0]!.$.button.click();
    clickDeleteMenuItem();
    flush();
    assertDeepEquals(['bar'], tabDiscardExceptionsList.$.list.items);

    const action =
        await performanceMetricsProxy.whenCalled('recordExceptionListAction');
    assertEquals(HighEfficiencyModeExceptionListAction.REMOVE, action);

    getExceptionListEntries()[0]!.$.button.click();
    clickDeleteMenuItem();
    flush();
    assertDeepEquals([], tabDiscardExceptionsList.$.list.items);
  });

  function openAddDialog(): TabDiscardExceptionDialogElement {
    let addDialog = getDialog();
    assertFalse(!!addDialog, 'dialog should not exist by default');

    tabDiscardExceptionsList.$.addButton.click();
    flush();

    addDialog = getDialog();
    assertTrue(
        !!addDialog, 'add dialog should exist after clicking add button');
    assertTrue(
        addDialog.$.dialog.open,
        'add dialog should be opened after clicking add button');
    assertEquals(
        '', addDialog.$.input.value,
        'add dialog input should be empty initially');
    return addDialog;
  }

  function openEditDialog(entry: TabDiscardExceptionEntryElement):
      TabDiscardExceptionDialogElement {
    let editDialog = getDialog();
    assertFalse(!!editDialog, 'dialog should not exist by default');

    entry.$.button.click();
    clickEditMenuItem();
    flush();

    editDialog = getDialog();
    assertTrue(
        !!editDialog, 'edit dialog should exist after clicking edit button');
    assertTrue(
        editDialog.$.dialog.open,
        'edit dialog should be opened after clicking edit button');
    assertEquals(
        entry.site, editDialog.$.input.value,
        'edit dialog input should be populated initially');
    return editDialog;
  }

  test('testTabDiscardExceptionsListAdd', async function() {
    setupExceptionListEntries(['foo']);
    const dialog = openAddDialog();
    dialog.fire(SUBMIT_EVENT, 'bar');

    const action =
        await performanceMetricsProxy.whenCalled('recordExceptionListAction');
    assertEquals(HighEfficiencyModeExceptionListAction.ADD, action);
    assertDeepEquals(
        ['foo', 'bar'], tabDiscardExceptionsList.$.list.items,
        'expected valid rule to be added to the end of the list');
  });

  test('testTabDiscardExceptionsListAddAfterMenuClick', function() {
    setupExceptionListEntries(['foo']);
    getExceptionListEntries()[0]!.$.button.click();
    tabDiscardExceptionsList.$.addButton.click();
    flush();

    const dialog = getDialog();
    assertTrue(!!dialog);
    assertEquals(
        '', dialog.$.input.value,
        'add dialog should be opened instead of edit dialog');
  });

  test('testTabDiscardExceptionsListEdit', async function() {
    setupExceptionListEntries(['foo', 'bar']);
    const dialog = openEditDialog(getExceptionListEntries()[1]!);
    dialog.fire(SUBMIT_EVENT, 'baz');

    const action =
        await performanceMetricsProxy.whenCalled('recordExceptionListAction');
    assertEquals(HighEfficiencyModeExceptionListAction.EDIT, action);
    assertDeepEquals(
        ['foo', 'baz'], tabDiscardExceptionsList.$.list.items,
        'expected valid rule to be added to the end of the list');
  });
});

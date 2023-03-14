// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrIconButtonElement} from 'chrome://settings/lazy_load.js';
import {HIGH_EFFICIENCY_MODE_PREF, HighEfficiencyModeExceptionListAction, PerformanceBrowserProxyImpl, PerformanceMetricsProxyImpl, SettingsPerformancePageElement, TAB_DISCARD_EXCEPTIONS_MANAGED_PREF, TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE, TAB_DISCARD_EXCEPTIONS_PREF, TabDiscardExceptionDialogElement, TabDiscardExceptionEntryElement, TabDiscardExceptionListElement} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestPerformanceBrowserProxy} from './test_performance_browser_proxy.js';
import {TestPerformanceMetricsProxy} from './test_performance_metrics_proxy.js';

suite('PerformancePage', function() {
  const CrPolicyStrings = {
    controlledSettingPolicy: 'policy',
  };
  let performancePage: SettingsPerformancePageElement;
  let performanceBrowserProxy: TestPerformanceBrowserProxy;
  let performanceMetricsProxy: TestPerformanceMetricsProxy;
  let tabDiscardExceptionsList: TabDiscardExceptionListElement;

  suiteSetup(function() {
    // Without this, cr-policy-pref-indicators will not have any text, making it
    // so that they cannot be shown.
    Object.assign(window, {CrPolicyStrings});
  });

  setup(function() {
    performanceBrowserProxy = new TestPerformanceBrowserProxy();
    PerformanceBrowserProxyImpl.setInstance(performanceBrowserProxy);

    performanceMetricsProxy = new TestPerformanceMetricsProxy();
    PerformanceMetricsProxyImpl.setInstance(performanceMetricsProxy);

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
          exceptions_managed: {
            enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
            controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
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

  function assertExceptionListEquals(rules: string[], message?: string) {
    const actual = tabDiscardExceptionsList.$.list.items!
                       .concat(tabDiscardExceptionsList.$.overflowList.items!)
                       .map(entry => entry.site);
    assertDeepEquals(rules, actual, message);
  }

  function setupExceptionListEntries(rules: string[], managedRules?: string[]) {
    if (managedRules) {
      performancePage.setPrefValue(
          TAB_DISCARD_EXCEPTIONS_MANAGED_PREF, managedRules);
    }
    performancePage.setPrefValue(TAB_DISCARD_EXCEPTIONS_PREF, rules);
    flush();
    assertExceptionListEquals([...managedRules ?? [], ...rules]);
  }

  function getExceptionListEntry(idx: number): TabDiscardExceptionEntryElement {
    const entry = [...tabDiscardExceptionsList.shadowRoot!
                       .querySelectorAll<TabDiscardExceptionEntryElement>(
                           'tab-discard-exception-entry')][idx];
    assertTrue(!!entry);
    return entry;
  }

  function clickMoreActionsButton(entry: TabDiscardExceptionEntryElement) {
    const button: CrIconButtonElement|null =
        entry.shadowRoot!.querySelector('cr-icon-button');
    assertTrue(!!button);
    button.click();
  }

  function clickDeleteMenuItem() {
    const button =
        tabDiscardExceptionsList.$.menu.get().querySelector<HTMLElement>(
            '#delete');
    assertTrue(!!button);
    button.click();
  }

  function clickEditMenuItem() {
    const button =
        tabDiscardExceptionsList.$.menu.get().querySelector<HTMLElement>(
            '#edit');
    assertTrue(!!button);
    button.click();
  }

  test('testTabDiscardExceptionsList', function() {
    // no sites added message should be shown when list is empty
    assertFalse(tabDiscardExceptionsList.$.noSitesAdded.hidden);
    assertExceptionListEquals([]);

    // list should be updated when pref is changed
    setupExceptionListEntries(['foo', 'bar']);
    assertTrue(tabDiscardExceptionsList.$.noSitesAdded.hidden);
  });

  test('testTabDiscardExceptionsManagedList', function() {
    const userRules = 3;
    const managedRules = 3;
    setupExceptionListEntries(
        [...Array(userRules).keys()].map(index => `user.rule${index}`),
        [...Array(managedRules).keys()].map(index => `managed.rule${index}`));

    const managedRule = getExceptionListEntry(0);
    assertTrue(managedRule.entry.managed);
    const indicator =
        managedRule.shadowRoot!.querySelector('cr-policy-pref-indicator');
    assertTrue(!!indicator);
    assertFalse(!!managedRule.shadowRoot!.querySelector('cr-icon-button'));

    const tooltip =
        tabDiscardExceptionsList.$.tooltip.shadowRoot!.querySelector(
            '#tooltip');
    assertTrue(!!tooltip);
    assertTrue(tooltip.classList.contains('hidden'));
    indicator.dispatchEvent(new Event('focus'));
    assertEquals(
        CrPolicyStrings.controlledSettingPolicy,
        tabDiscardExceptionsList.$.tooltip.textContent!.trim());
    assertFalse(tooltip.classList.contains('hidden'));
    assertEquals(indicator, tabDiscardExceptionsList.$.tooltip.target);

    const userRule = getExceptionListEntry(managedRules);
    assertFalse(userRule.entry.managed);
    assertFalse(
        !!userRule.shadowRoot!.querySelector('cr-policy-pref-indicator'));
    assertTrue(!!userRule.shadowRoot!.querySelector('cr-icon-button'));
  });

  test('testTabDiscardExceptionsListDelete', async function() {
    setupExceptionListEntries(['foo', 'bar']);

    clickMoreActionsButton(getExceptionListEntry(0));
    clickDeleteMenuItem();
    flush();
    assertExceptionListEquals(['bar']);
    assertEquals(
        HighEfficiencyModeExceptionListAction.REMOVE,
        await performanceMetricsProxy.whenCalled('recordExceptionListAction'));

    clickMoreActionsButton(getExceptionListEntry(0));
    clickDeleteMenuItem();
    flush();
    assertExceptionListEquals([]);
  });

  function getDialog(): TabDiscardExceptionDialogElement|null {
    return tabDiscardExceptionsList.shadowRoot!.querySelector(
        'tab-discard-exception-dialog');
  }

  async function inputDialog(
      dialog: TabDiscardExceptionDialogElement, input: string) {
    const inputEvent = eventToPromise('input', dialog.$.input);
    dialog.$.input.value = input;
    dialog.$.input.dispatchEvent(new CustomEvent('input'));
    await inputEvent;
    dialog.$.actionButton.click();
  }

  test('testTabDiscardExceptionsListAdd', async function() {
    setupExceptionListEntries(['foo']);
    let addDialog = getDialog();
    assertFalse(!!addDialog);

    tabDiscardExceptionsList.$.addButton.click();
    flush();

    addDialog = getDialog();
    assertTrue(!!addDialog);
    assertTrue(addDialog.$.dialog.open);
    assertEquals('', addDialog.$.input.value);
    await inputDialog(addDialog, 'bar');
    assertExceptionListEquals(['foo', 'bar']);
  });

  test('testTabDiscardExceptionsListEdit', async function() {
    setupExceptionListEntries(['foo', 'bar']);
    const entry = getExceptionListEntry(1);
    let editDialog = getDialog();
    assertFalse(!!editDialog);

    clickMoreActionsButton(entry);
    clickEditMenuItem();
    flush();

    editDialog = getDialog();
    assertTrue(!!editDialog);
    assertTrue(editDialog.$.dialog.open);
    assertEquals(entry.entry.site, editDialog.$.input.value);
    await inputDialog(editDialog, 'baz');
    assertExceptionListEquals(['foo', 'baz']);
  });

  test('testTabDiscardExceptionsListAddAfterMenuClick', function() {
    setupExceptionListEntries(['foo']);
    clickMoreActionsButton(getExceptionListEntry(0));
    tabDiscardExceptionsList.$.addButton.click();
    flush();

    const dialog = getDialog();
    assertTrue(!!dialog);
    assertEquals('', dialog.$.input.value);
  });

  test('testTabDiscardExceptionsListOverflow', async function() {
    assertTrue(tabDiscardExceptionsList.$.expandButton.hidden);

    const entries = [
      ...Array(TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE + 1).keys(),
    ].map(index => `rule${index}`);
    setupExceptionListEntries([...entries]);
    assertFalse(tabDiscardExceptionsList.$.collapse.opened);
    assertFalse(tabDiscardExceptionsList.$.expandButton.hidden);

    tabDiscardExceptionsList.$.expandButton.click();
    assertTrue(tabDiscardExceptionsList.$.collapse.opened);

    tabDiscardExceptionsList.$.expandButton.click();
    assertFalse(tabDiscardExceptionsList.$.collapse.opened);

    tabDiscardExceptionsList.$.addButton.click();
    flush();

    const newRule = `rule${TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE + 1}`;
    const addDialog = getDialog();
    assertTrue(!!addDialog);
    await inputDialog(addDialog, newRule);
    assertTrue(tabDiscardExceptionsList.$.collapse.opened);
    assertExceptionListEquals([...entries, newRule]);
  });

  test('testTabDiscardExceptionsListOverflowEdit', async function() {
    const entries = [
      ...Array(TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE + 1).keys(),
    ].map(index => `rule${index}`);
    setupExceptionListEntries([...entries]);

    const entry = getExceptionListEntry(TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE);
    clickMoreActionsButton(entry);
    clickEditMenuItem();
    flush();
    const editDialog = getDialog();
    assertTrue(!!editDialog);
    assertEquals(entry.entry.site, editDialog.$.input.value);
    await inputDialog(editDialog, 'foo');
    assertExceptionListEquals([...entries.slice(0, -1), 'foo']);

    clickMoreActionsButton(entry);
    clickEditMenuItem();
    flush();
    assertTrue(!!editDialog);
    await inputDialog(editDialog, getExceptionListEntry(0).entry.site);
    assertExceptionListEquals(entries.slice(0, -1));
  });

  test('testTabDiscardExceptionsListOverflowDelete', function() {
    const entries = [
      ...Array(TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE + 2).keys(),
    ].map(index => `rule${index}`);
    setupExceptionListEntries([...entries]);

    let entry =
        getExceptionListEntry(TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE + 1);
    clickMoreActionsButton(entry);
    clickDeleteMenuItem();
    flush();
    assertExceptionListEquals(entries.slice(0, -1));

    entry = getExceptionListEntry(TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE);
    clickMoreActionsButton(entry);
    clickDeleteMenuItem();
    flush();
    assertExceptionListEquals(entries.slice(0, -2));
  });
});

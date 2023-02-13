// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrIconButtonElement} from 'chrome://settings/lazy_load.js';
import {HIGH_EFFICIENCY_MODE_PREF, HighEfficiencyModeExceptionListAction, PerformanceBrowserProxyImpl, PerformanceMetricsProxyImpl, SettingsPerformancePageElement, TAB_DISCARD_EXCEPTIONS_MANAGED_PREF, TAB_DISCARD_EXCEPTIONS_PREF, TabDiscardExceptionDialogElement, TabDiscardExceptionEntryElement, TabDiscardExceptionListElement} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

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
    assertDeepEquals(
        rules, tabDiscardExceptionsList.$.list.items!.map(entry => entry.site),
        message);
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
                           'tab-discard-exception-entry:not([hidden])')][idx];
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

  test('testTabDiscardExceptionsManagedList', function(done) {
    const userRules = 3;
    const managedRules = 3;
    // Need to wait until updateScrollableContents updates the list with the
    // correct items before making assertions on them.
    const resizeListener = function() {
      flush();
      if (tabDiscardExceptionsList.shadowRoot!
              .querySelectorAll('tab-discard-exception-entry:not([hidden])')
              .length !== userRules + managedRules) {
        return;
      }

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

      tabDiscardExceptionsList.removeEventListener(
          'iron-resize', resizeListener);
      done();
    };
    tabDiscardExceptionsList.addEventListener('iron-resize', resizeListener);
    setupExceptionListEntries(
        [...Array(userRules).keys()].map(index => `user.rule${index}`),
        [...Array(managedRules).keys()].map(index => `managed.rule${index}`));
  });


  test('testTabDiscardExceptionsListDelete', async function() {
    setupExceptionListEntries(['foo', 'bar']);

    clickMoreActionsButton(getExceptionListEntry(0));
    clickDeleteMenuItem();
    flush();
    assertExceptionListEquals(['bar']);

    const action =
        await performanceMetricsProxy.whenCalled('recordExceptionListAction');
    assertEquals(HighEfficiencyModeExceptionListAction.REMOVE, action);

    clickMoreActionsButton(getExceptionListEntry(0));
    clickDeleteMenuItem();
    flush();
    assertExceptionListEquals([]);
  });

  function getDialog(): TabDiscardExceptionDialogElement|null {
    return tabDiscardExceptionsList.shadowRoot!.querySelector(
        'tab-discard-exception-dialog');
  }

  test('testTabDiscardExceptionsListAdd', async function() {
    setupExceptionListEntries(['foo']);
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
  });

  test('testTabDiscardExceptionsListEdit', async function() {
    setupExceptionListEntries(['foo', 'bar']);
    const entry = getExceptionListEntry(1);
    let editDialog = getDialog();
    assertFalse(!!editDialog, 'dialog should not exist by default');

    clickMoreActionsButton(entry);
    clickEditMenuItem();
    flush();

    editDialog = getDialog();
    assertTrue(
        !!editDialog, 'edit dialog should exist after clicking edit button');
    assertTrue(
        editDialog.$.dialog.open,
        'edit dialog should be opened after clicking edit button');
    assertEquals(
        entry.entry.site, editDialog.$.input.value,
        'edit dialog input should be populated initially');
  });

  test('testTabDiscardExceptionsListAddAfterMenuClick', function() {
    setupExceptionListEntries(['foo']);
    clickMoreActionsButton(getExceptionListEntry(0));
    tabDiscardExceptionsList.$.addButton.click();
    flush();

    const dialog = getDialog();
    assertTrue(!!dialog);
    assertEquals(
        '', dialog.$.input.value,
        'add dialog should be opened instead of edit dialog');
  });
});

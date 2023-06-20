// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';
import 'chrome://settings/settings.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrIconButtonElement, IronCollapseElement, SettingsRadioGroupElement} from 'chrome://settings/lazy_load.js';
import {HIGH_EFFICIENCY_MODE_PREF, HighEfficiencyModeExceptionListAction, HighEfficiencyModeState, PerformanceBrowserProxyImpl, PerformanceMetricsProxyImpl, SettingsDropdownMenuElement, SettingsPerformancePageElement, TAB_DISCARD_EXCEPTIONS_MANAGED_PREF, TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE, TAB_DISCARD_EXCEPTIONS_PREF, TabDiscardExceptionAddDialogElement, TabDiscardExceptionCurrentSitesEntryElement, TabDiscardExceptionEditDialogElement, TabDiscardExceptionEntryElement, TabDiscardExceptionListElement, TabDiscardExceptionTabbedAddDialogElement} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestPerformanceBrowserProxy} from './test_performance_browser_proxy.js';
import {TestPerformanceMetricsProxy} from './test_performance_metrics_proxy.js';

const highEfficiencyModeDummyPrefs = {
  high_efficiency_mode: {
    state: {
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: HighEfficiencyModeState.DISABLED,
    },
    time_before_discard_in_minutes: {
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 1,
    },
  },
};

/**
 * Constructs dummy prefs for tab discarding. Needs to be a function so that
 * list pref values are recreated and not shared between test suites.
 */
function tabDiscardingDummyPrefs(): Record<
    string, Record<string, Omit<chrome.settingsPrivate.PrefObject, 'key'>>> {
  return {
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
  };
}

suite('PerformancePage', function() {
  let performancePage: SettingsPerformancePageElement;
  let performanceMetricsProxy: TestPerformanceMetricsProxy;

  setup(function() {
    performanceMetricsProxy = new TestPerformanceMetricsProxy();
    PerformanceMetricsProxyImpl.setInstance(performanceMetricsProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    performancePage = document.createElement('settings-performance-page');
    performancePage.set('prefs', {
      performance_tuning: {
        ...highEfficiencyModeDummyPrefs,
        ...tabDiscardingDummyPrefs(),
      },
    });
    document.body.appendChild(performancePage);
    flush();
  });

  test('testHighEfficiencyModeEnabled', function() {
    performancePage.setPrefValue(
        HIGH_EFFICIENCY_MODE_PREF, HighEfficiencyModeState.ENABLED_ON_TIMER);
    assertTrue(performancePage.$.toggleButton.checked);
  });

  test('testHighEfficiencyModeDisabled', function() {
    performancePage.setPrefValue(
        HIGH_EFFICIENCY_MODE_PREF, HighEfficiencyModeState.DISABLED);
    assertFalse(performancePage.$.toggleButton.checked);
  });

  test('testHighEfficiencyModeChangeState', async function() {
    performancePage.setPrefValue(
        HIGH_EFFICIENCY_MODE_PREF, HighEfficiencyModeState.DISABLED);

    performancePage.$.toggleButton.click();
    let state = await performanceMetricsProxy.whenCalled(
        'recordHighEfficiencyModeChanged');
    assertEquals(state, HighEfficiencyModeState.ENABLED_ON_TIMER);
    assertEquals(
        performancePage.getPref(HIGH_EFFICIENCY_MODE_PREF).value,
        HighEfficiencyModeState.ENABLED_ON_TIMER);

    performanceMetricsProxy.reset();
    performancePage.$.toggleButton.click();
    state = await performanceMetricsProxy.whenCalled(
        'recordHighEfficiencyModeChanged');
    assertEquals(state, HighEfficiencyModeState.DISABLED);
    assertEquals(
        performancePage.getPref(HIGH_EFFICIENCY_MODE_PREF).value,
        HighEfficiencyModeState.DISABLED);
  });
});

suite('PerformancePageMultistate', function() {
  let performancePage: SettingsPerformancePageElement;
  let performanceMetricsProxy: TestPerformanceMetricsProxy;
  let enabledOnTimerButton: HTMLElement;
  let radioGroup: SettingsRadioGroupElement;
  let radioGroupCollapse: IronCollapseElement;
  let discardTimeDropdown: SettingsDropdownMenuElement;

  const DISCARD_TIME_PREF =
      'performance_tuning.high_efficiency_mode.time_before_discard_in_minutes';

  /**
   * Used to get elements form the performance page that may or may not exist,
   * such as those inside a dom-if.
   * TODO(charlesmeng): remove once kHighEfficiencyMultistateMode flag is
   * cleaned up, since elements can then be selected with $ interface
   */
  function getPerformancePageElement<T extends HTMLElement = HTMLElement>(
      id: string): T {
    const el = performancePage.shadowRoot!.querySelector<T>(`#${id}`);
    assertTrue(!!el);
    assertTrue(el instanceof HTMLElement);
    return el;
  }

  setup(function() {
    performanceMetricsProxy = new TestPerformanceMetricsProxy();
    PerformanceMetricsProxyImpl.setInstance(performanceMetricsProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    performancePage = document.createElement('settings-performance-page');
    performancePage.set('prefs', {
      performance_tuning: {
        ...highEfficiencyModeDummyPrefs,
        ...tabDiscardingDummyPrefs(),
      },
    });
    document.body.appendChild(performancePage);
    flush();

    enabledOnTimerButton = getPerformancePageElement('enabledOnTimerButton');
    radioGroup = getPerformancePageElement('radioGroup');
    radioGroupCollapse = getPerformancePageElement('radioGroupCollapse');
    discardTimeDropdown = getPerformancePageElement('discardTimeDropdown');
  });

  test('testHighEfficiencyModeDisabled', function() {
    performancePage.setPrefValue(
        HIGH_EFFICIENCY_MODE_PREF, HighEfficiencyModeState.DISABLED);
    assertFalse(performancePage.$.toggleButton.checked);
    assertFalse(radioGroupCollapse.opened);
    assertTrue(discardTimeDropdown.disabled);
  });

  test('testHighEfficiencyModeEnabled', function() {
    performancePage.setPrefValue(
        HIGH_EFFICIENCY_MODE_PREF, HighEfficiencyModeState.ENABLED);
    assertTrue(performancePage.$.toggleButton.checked);
    assertTrue(radioGroupCollapse.opened);
    assertEquals(String(HighEfficiencyModeState.ENABLED), radioGroup.selected);
    assertTrue(discardTimeDropdown.disabled);
  });

  test('testHighEfficiencyModeEnabledOnTimer', function() {
    performancePage.setPrefValue(
        HIGH_EFFICIENCY_MODE_PREF, HighEfficiencyModeState.ENABLED_ON_TIMER);
    assertTrue(performancePage.$.toggleButton.checked);
    assertTrue(radioGroupCollapse.opened);
    assertEquals(
        String(HighEfficiencyModeState.ENABLED_ON_TIMER), radioGroup.selected);
    assertFalse(discardTimeDropdown.disabled);
  });

  test('testHighEfficiencyModeDiscardTime', async function() {
    performancePage.setPrefValue(
        HIGH_EFFICIENCY_MODE_PREF, HighEfficiencyModeState.ENABLED_ON_TIMER);
    performancePage.setPrefValue(DISCARD_TIME_PREF, 120);
    // Need to wait for dropdown menu to update its selection using a microtask
    await flushTasks();
    assertTrue(!!discardTimeDropdown.$.dropdownMenu.options[4]);
    assertTrue(discardTimeDropdown.$.dropdownMenu.options[4].selected);
    assertEquals(
        performancePage.getPref(DISCARD_TIME_PREF).value,
        Number(discardTimeDropdown.$.dropdownMenu.value));

    assertTrue(!!discardTimeDropdown.$.dropdownMenu.options[3]);
    const newDiscardTime = discardTimeDropdown.$.dropdownMenu.options[3].value;
    discardTimeDropdown.$.dropdownMenu.options[3].selected = true;
    discardTimeDropdown.$.dropdownMenu.dispatchEvent(new CustomEvent('change'));
    assertEquals(
        Number(newDiscardTime),
        performancePage.getPref(DISCARD_TIME_PREF).value);
  });

  test('testHighEfficiencyModeChangeState', async function() {
    performancePage.setPrefValue(
        HIGH_EFFICIENCY_MODE_PREF, HighEfficiencyModeState.DISABLED);

    performancePage.$.toggleButton.click();
    let state = await performanceMetricsProxy.whenCalled(
        'recordHighEfficiencyModeChanged');
    assertEquals(state, HighEfficiencyModeState.ENABLED);
    assertEquals(
        performancePage.getPref(HIGH_EFFICIENCY_MODE_PREF).value,
        HighEfficiencyModeState.ENABLED);

    performanceMetricsProxy.reset();
    enabledOnTimerButton.click();
    state = await performanceMetricsProxy.whenCalled(
        'recordHighEfficiencyModeChanged');
    assertEquals(state, HighEfficiencyModeState.ENABLED_ON_TIMER);
    assertEquals(
        performancePage.getPref(HIGH_EFFICIENCY_MODE_PREF).value,
        HighEfficiencyModeState.ENABLED_ON_TIMER);

    performanceMetricsProxy.reset();
    performancePage.$.toggleButton.click();
    state = await performanceMetricsProxy.whenCalled(
        'recordHighEfficiencyModeChanged');
    assertEquals(state, HighEfficiencyModeState.DISABLED);
    assertEquals(
        performancePage.getPref(HIGH_EFFICIENCY_MODE_PREF).value,
        HighEfficiencyModeState.DISABLED);
  });
});

suite('TabDiscardExceptionList', function() {
  const CrPolicyStrings = {
    controlledSettingPolicy: 'policy',
  };
  let performancePage: SettingsPerformancePageElement;
  let performanceBrowserProxy: TestPerformanceBrowserProxy;
  let performanceMetricsProxy: TestPerformanceMetricsProxy;
  let tabDiscardExceptionsList: TabDiscardExceptionListElement;

  suiteSetup(function() {
    // Without this, cr-policy-pref-indicator will not have any text, making it
    // so that it cannot be shown.
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
      performance_tuning: tabDiscardingDummyPrefs(),
    });
    document.body.appendChild(performancePage);
    flush();

    tabDiscardExceptionsList = performancePage.$.tabDiscardExceptionsList;
  });

  function assertExceptionListEquals(rules: string[], message?: string) {
    const actual = tabDiscardExceptionsList.$.list.items!
                       .concat(tabDiscardExceptionsList.$.overflowList.items!)
                       .map(entry => entry.site)
                       .reverse();
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
    const entries = [...tabDiscardExceptionsList.shadowRoot!
                         .querySelectorAll<TabDiscardExceptionEntryElement>(
                             'tab-discard-exception-entry')];
    const entry = entries[entries.length - 1 - idx];
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

  async function getAddDialog():
      Promise<TabDiscardExceptionAddDialogElement|
              TabDiscardExceptionTabbedAddDialogElement> {
    if (loadTimeData.getBoolean('isDiscardExceptionsImprovementsEnabled')) {
      return await getTabbedAddDialog();
    }
    const dialog = tabDiscardExceptionsList.shadowRoot!.querySelector(
        'tab-discard-exception-add-dialog');
    assertTrue(!!dialog);
    return dialog;
  }

  async function getTabbedAddDialog():
      Promise<TabDiscardExceptionTabbedAddDialogElement> {
    await performanceBrowserProxy.whenCalled('getCurrentOpenSites');
    const dialog = tabDiscardExceptionsList.shadowRoot!.querySelector(
        'tab-discard-exception-tabbed-add-dialog');
    assertTrue(!!dialog);
    return dialog;
  }

  function getEditDialog(): TabDiscardExceptionEditDialogElement {
    const dialog = tabDiscardExceptionsList.shadowRoot!.querySelector(
        'tab-discard-exception-edit-dialog');
    assertTrue(!!dialog);
    return dialog;
  }

  function assertAddDialogDoesNotExist() {
    if (loadTimeData.getBoolean('isDiscardExceptionsImprovementsEnabled')) {
      assertTabbedAddDialogDoesNotExist();
    }
    const dialog = tabDiscardExceptionsList.shadowRoot!.querySelector(
        'tab-discard-exception-add-dialog');
    assertFalse(!!dialog);
  }

  function assertTabbedAddDialogDoesNotExist() {
    assertEquals(
        0, performanceBrowserProxy.getCallCount('getCurrentOpenSites'));
    const dialog = tabDiscardExceptionsList.shadowRoot!.querySelector(
        'tab-discard-exception-tabbed-add-dialog');
    assertFalse(!!dialog);
  }

  function assertEditDialogDoesNotExist() {
    const dialog = tabDiscardExceptionsList.shadowRoot!.querySelector(
        'tab-discard-exception-edit-dialog');
    assertFalse(!!dialog);
  }

  async function inputDialog(
      dialog: TabDiscardExceptionAddDialogElement|
      TabDiscardExceptionTabbedAddDialogElement|
      TabDiscardExceptionEditDialogElement,
      input: string) {
    const inputEvent = eventToPromise('input', dialog.$.input.$.input);
    dialog.$.input.$.input.value = input;
    dialog.$.input.$.input.dispatchEvent(new CustomEvent('input'));
    await inputEvent;
    dialog.$.actionButton.click();
  }

  test('testTabDiscardExceptionsListAdd', async function() {
    setupExceptionListEntries(['foo']);
    assertAddDialogDoesNotExist();

    tabDiscardExceptionsList.$.addButton.click();
    flush();

    const addDialog = await getAddDialog();
    assertTrue(addDialog.$.dialog.open);
    assertEquals('', addDialog.$.input.$.input.value);
    await inputDialog(addDialog, 'bar');
    assertEquals(
        HighEfficiencyModeExceptionListAction.ADD_MANUAL,
        await performanceMetricsProxy.whenCalled('recordExceptionListAction'));
    assertExceptionListEquals(['foo', 'bar']);
  });

  test('testTabDiscardExceptionsListEdit', async function() {
    setupExceptionListEntries(['foo', 'bar']);
    const entry = getExceptionListEntry(1);
    assertEditDialogDoesNotExist();

    clickMoreActionsButton(entry);
    clickEditMenuItem();
    flush();

    const editDialog = getEditDialog();
    assertTrue(editDialog.$.dialog.open);
    assertEquals(entry.entry.site, editDialog.$.input.$.input.value);
    await inputDialog(editDialog, 'baz');
    assertEquals(
        HighEfficiencyModeExceptionListAction.EDIT,
        await performanceMetricsProxy.whenCalled('recordExceptionListAction'));
    assertExceptionListEquals(['foo', 'baz']);
  });

  test('testTabDiscardExceptionsListAddAfterMenuClick', async function() {
    setupExceptionListEntries(['foo']);
    clickMoreActionsButton(getExceptionListEntry(0));
    tabDiscardExceptionsList.$.addButton.click();
    flush();

    const addDialog = await getAddDialog();
    assertEquals('', addDialog.$.input.$.input.value);
  });

  test('testTabDiscardExceptionsListAddExceptionOverflow', async function() {
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
    const addDialog = await getAddDialog();
    await inputDialog(addDialog, newRule);
    assertFalse(tabDiscardExceptionsList.$.collapse.opened);
    assertExceptionListEquals([...entries, newRule]);
  });

  test('testTabDiscardExceptionsListAddExceptionsOverflow', async function() {
    if (!loadTimeData.getBoolean('isDiscardExceptionsImprovementsEnabled')) {
      return;
    }
    const existingEntry = 'www.foo.com';
    setupExceptionListEntries([existingEntry]);
    const entries = [
      ...Array(TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE).keys(),
    ].map(index => `rule${index}`);
    performanceBrowserProxy.setCurrentOpenSites(entries);
    tabDiscardExceptionsList.$.addButton.click();
    flush();

    const addDialog = await getTabbedAddDialog();
    await eventToPromise('iron-resize', addDialog);
    flush();

    addDialog.$.list.$.list
        .querySelectorAll<TabDiscardExceptionCurrentSitesEntryElement>(
            'tab-discard-exception-current-sites-entry:not([hidden])')
        .forEach(currentSitesEntryElement => {
          currentSitesEntryElement.click();
        });
    assertFalse(addDialog.$.actionButton.disabled);
    addDialog.$.actionButton.click();
    flush();

    assertEquals(false, tabDiscardExceptionsList.$.collapse.opened);
    assertExceptionListEquals([existingEntry, ...entries]);
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
    const editDialog = getEditDialog();
    assertEquals(entry.entry.site, editDialog.$.input.$.input.value);
    await inputDialog(editDialog, 'foo');
    assertExceptionListEquals([...entries.slice(0, -1), 'foo']);

    clickMoreActionsButton(entry);
    clickEditMenuItem();
    flush();
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

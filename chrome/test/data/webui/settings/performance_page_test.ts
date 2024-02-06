// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';
import 'chrome://settings/settings.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrIconButtonElement, IronCollapseElement, SettingsRadioGroupElement} from 'chrome://settings/lazy_load.js';
import type {ExceptionAddDialogElement, ExceptionEditDialogElement, ExceptionEntryElement, ExceptionListElement, ExceptionTabbedAddDialogElement, SettingsCheckboxListEntryElement, SettingsDropdownMenuElement, SettingsPerformancePageElement} from 'chrome://settings/settings.js';
import {convertDateToWindowsEpoch, MEMORY_SAVER_MODE_PREF, MemorySaverModeExceptionListAction, MemorySaverModeState, PerformanceBrowserProxyImpl, PerformanceMetricsProxyImpl, TAB_DISCARD_EXCEPTIONS_MANAGED_PREF, TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE, TAB_DISCARD_EXCEPTIONS_PREF} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestPerformanceBrowserProxy} from './test_performance_browser_proxy.js';
import {TestPerformanceMetricsProxy} from './test_performance_metrics_proxy.js';

const memorySaverModeMockPrefs = {
  high_efficiency_mode: {
    state: {
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: MemorySaverModeState.DISABLED,
    },
    time_before_discard_in_minutes: {
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 1,
    },
  },
};

/**
 * Constructs mock prefs for tab discarding. Needs to be a function so that
 * list pref values are recreated and not shared between test suites.
 */
function tabDiscardingMockPrefs(): Record<
    string, Record<string, Omit<chrome.settingsPrivate.PrefObject, 'key'>>> {
  return {
    tab_discarding: {
      exceptions_with_time: {
        type: chrome.settingsPrivate.PrefType.DICTIONARY,
        value: {},
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
        ...memorySaverModeMockPrefs,
        ...tabDiscardingMockPrefs(),
      },
    });
    document.body.appendChild(performancePage);
    flush();
  });

  test('testMemorySaverModeEnabled', function() {
    performancePage.setPrefValue(
        MEMORY_SAVER_MODE_PREF, MemorySaverModeState.ENABLED_ON_TIMER);
    assertTrue(performancePage.$.toggleButton.checked);
  });

  test('testMemorySaverModeDisabled', function() {
    performancePage.setPrefValue(
        MEMORY_SAVER_MODE_PREF, MemorySaverModeState.DISABLED);
    assertFalse(performancePage.$.toggleButton.checked);
  });

  test('testMemorySaverModeChangeState', async function() {
    performancePage.setPrefValue(
        MEMORY_SAVER_MODE_PREF, MemorySaverModeState.DISABLED);

    performancePage.$.toggleButton.click();
    let state = await performanceMetricsProxy.whenCalled(
        'recordMemorySaverModeChanged');
    assertEquals(state, MemorySaverModeState.ENABLED_ON_TIMER);
    assertEquals(
        performancePage.getPref(MEMORY_SAVER_MODE_PREF).value,
        MemorySaverModeState.ENABLED_ON_TIMER);

    performanceMetricsProxy.reset();
    performancePage.$.toggleButton.click();
    state = await performanceMetricsProxy.whenCalled(
        'recordMemorySaverModeChanged');
    assertEquals(state, MemorySaverModeState.DISABLED);
    assertEquals(
        performancePage.getPref(MEMORY_SAVER_MODE_PREF).value,
        MemorySaverModeState.DISABLED);
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
   * TODO(charlesmeng): remove once kMemorySaverMultistateMode flag is
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
        ...memorySaverModeMockPrefs,
        ...tabDiscardingMockPrefs(),
      },
    });
    document.body.appendChild(performancePage);
    flush();

    enabledOnTimerButton = getPerformancePageElement('enabledOnTimerButton');
    radioGroup = getPerformancePageElement('radioGroup');
    radioGroupCollapse = getPerformancePageElement('radioGroupCollapse');
    discardTimeDropdown = getPerformancePageElement('discardTimeDropdown');
  });

  test('testMemorySaverModeDisabled', function() {
    performancePage.setPrefValue(
        MEMORY_SAVER_MODE_PREF, MemorySaverModeState.DISABLED);
    assertFalse(performancePage.$.toggleButton.checked);
    assertFalse(radioGroupCollapse.opened);
    assertTrue(discardTimeDropdown.disabled);
  });

  test('testMemorySaverModeEnabled', function() {
    performancePage.setPrefValue(
        MEMORY_SAVER_MODE_PREF, MemorySaverModeState.ENABLED);
    assertTrue(performancePage.$.toggleButton.checked);
    assertTrue(radioGroupCollapse.opened);
    assertEquals(String(MemorySaverModeState.ENABLED), radioGroup.selected);
    assertTrue(discardTimeDropdown.disabled);
  });

  test('testMemorySaverModeEnabledOnTimer', function() {
    performancePage.setPrefValue(
        MEMORY_SAVER_MODE_PREF, MemorySaverModeState.ENABLED_ON_TIMER);
    assertTrue(performancePage.$.toggleButton.checked);
    assertTrue(radioGroupCollapse.opened);
    assertEquals(
        String(MemorySaverModeState.ENABLED_ON_TIMER), radioGroup.selected);
    assertFalse(discardTimeDropdown.disabled);
  });

  test('testMemorySaverModeDiscardTime', async function() {
    performancePage.setPrefValue(
        MEMORY_SAVER_MODE_PREF, MemorySaverModeState.ENABLED_ON_TIMER);
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

  test('testMemorySaverModeChangeState', async function() {
    performancePage.setPrefValue(
        MEMORY_SAVER_MODE_PREF, MemorySaverModeState.DISABLED);

    performancePage.$.toggleButton.click();
    let state = await performanceMetricsProxy.whenCalled(
        'recordMemorySaverModeChanged');
    assertEquals(state, MemorySaverModeState.ENABLED);
    assertEquals(
        performancePage.getPref(MEMORY_SAVER_MODE_PREF).value,
        MemorySaverModeState.ENABLED);

    performanceMetricsProxy.reset();
    enabledOnTimerButton.click();
    state = await performanceMetricsProxy.whenCalled(
        'recordMemorySaverModeChanged');
    assertEquals(state, MemorySaverModeState.ENABLED_ON_TIMER);
    assertEquals(
        performancePage.getPref(MEMORY_SAVER_MODE_PREF).value,
        MemorySaverModeState.ENABLED_ON_TIMER);

    performanceMetricsProxy.reset();
    performancePage.$.toggleButton.click();
    state = await performanceMetricsProxy.whenCalled(
        'recordMemorySaverModeChanged');
    assertEquals(state, MemorySaverModeState.DISABLED);
    assertEquals(
        performancePage.getPref(MEMORY_SAVER_MODE_PREF).value,
        MemorySaverModeState.DISABLED);
  });
});

suite('TabDiscardExceptionList', function() {
  const CrPolicyStrings = {
    controlledSettingPolicy: 'policy',
  };
  let performancePage: SettingsPerformancePageElement;
  let performanceBrowserProxy: TestPerformanceBrowserProxy;
  let performanceMetricsProxy: TestPerformanceMetricsProxy;
  let exceptionList: ExceptionListElement;

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
      performance_tuning: tabDiscardingMockPrefs(),
    });
    document.body.appendChild(performancePage);
    flush();

    exceptionList = performancePage.$.exceptionList;
  });

  function assertExceptionListEquals(rules: string[], message?: string) {
    const actual =
        exceptionList.$.list.items!.concat(exceptionList.$.overflowList.items!)
            .map(entry => entry.site)
            .reverse();
    assertDeepEquals(rules, actual, message);
  }

  function setupExceptionListEntries(rules: string[], managedRules?: string[]) {
    if (managedRules) {
      performancePage.setPrefValue(
          TAB_DISCARD_EXCEPTIONS_MANAGED_PREF, managedRules);
    }
    performancePage.setPrefValue(
        TAB_DISCARD_EXCEPTIONS_PREF,
        Object.fromEntries(rules.map(r => [r, convertDateToWindowsEpoch()])));
    flush();
    assertExceptionListEquals([...managedRules ?? [], ...rules]);
  }

  function getExceptionListEntry(idx: number): ExceptionEntryElement {
    const entries =
        [...exceptionList.shadowRoot!.querySelectorAll<ExceptionEntryElement>(
            'tab-discard-exception-entry')];
    const entry = entries[entries.length - 1 - idx];
    assertTrue(!!entry);
    return entry;
  }

  function clickMoreActionsButton(entry: ExceptionEntryElement) {
    const button: CrIconButtonElement|null =
        entry.shadowRoot!.querySelector('cr-icon-button');
    assertTrue(!!button);
    button.click();
  }

  function clickDeleteMenuItem() {
    const button =
        exceptionList.$.menu.get().querySelector<HTMLElement>('#delete');
    assertTrue(!!button);
    button.click();
  }

  function clickEditMenuItem() {
    const button =
        exceptionList.$.menu.get().querySelector<HTMLElement>('#edit');
    assertTrue(!!button);
    button.click();
  }

  test('testExceptionList', function() {
    // no sites added message should be shown when list is empty
    assertFalse(exceptionList.$.noSitesAdded.hidden);
    assertExceptionListEquals([]);

    // list should be updated when pref is changed
    setupExceptionListEntries(['foo', 'bar']);
    assertTrue(exceptionList.$.noSitesAdded.hidden);
  });

  test('testManagedExceptionList', function() {
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
        exceptionList.$.tooltip.shadowRoot!.querySelector('#tooltip');
    assertTrue(!!tooltip);
    assertTrue(tooltip.classList.contains('hidden'));
    indicator.dispatchEvent(new Event('focus'));
    assertEquals(
        CrPolicyStrings.controlledSettingPolicy,
        exceptionList.$.tooltip.textContent!.trim());
    assertFalse(tooltip.classList.contains('hidden'));
    assertEquals(indicator, exceptionList.$.tooltip.target);

    const userRule = getExceptionListEntry(managedRules);
    assertFalse(userRule.entry.managed);
    assertFalse(
        !!userRule.shadowRoot!.querySelector('cr-policy-pref-indicator'));
    assertTrue(!!userRule.shadowRoot!.querySelector('cr-icon-button'));
  });

  test('testExceptionListDelete', async function() {
    setupExceptionListEntries(['foo', 'bar']);

    clickMoreActionsButton(getExceptionListEntry(0));
    clickDeleteMenuItem();
    flush();
    assertExceptionListEquals(['bar']);
    assertEquals(
        MemorySaverModeExceptionListAction.REMOVE,
        await performanceMetricsProxy.whenCalled('recordExceptionListAction'));

    clickMoreActionsButton(getExceptionListEntry(0));
    clickDeleteMenuItem();
    flush();
    assertExceptionListEquals([]);
  });

  async function getAddDialog():
      Promise<ExceptionAddDialogElement|ExceptionTabbedAddDialogElement> {
    if (loadTimeData.getBoolean('isDiscardExceptionsImprovementsEnabled')) {
      return await getTabbedAddDialog();
    }
    const dialog = exceptionList.shadowRoot!.querySelector(
        'tab-discard-exception-add-dialog');
    assertTrue(!!dialog);
    return dialog;
  }

  async function getTabbedAddDialog():
      Promise<ExceptionTabbedAddDialogElement> {
    await performanceBrowserProxy.whenCalled('getCurrentOpenSites');
    const dialog = exceptionList.shadowRoot!.querySelector(
        'tab-discard-exception-tabbed-add-dialog');
    assertTrue(!!dialog);
    return dialog;
  }

  function getEditDialog(): ExceptionEditDialogElement {
    const dialog = exceptionList.shadowRoot!.querySelector(
        'tab-discard-exception-edit-dialog');
    assertTrue(!!dialog);
    return dialog;
  }

  function assertAddDialogDoesNotExist() {
    if (loadTimeData.getBoolean('isDiscardExceptionsImprovementsEnabled')) {
      assertTabbedAddDialogDoesNotExist();
    }
    const dialog = exceptionList.shadowRoot!.querySelector(
        'tab-discard-exception-add-dialog');
    assertFalse(!!dialog);
  }

  function assertTabbedAddDialogDoesNotExist() {
    assertEquals(
        0, performanceBrowserProxy.getCallCount('getCurrentOpenSites'));
    const dialog = exceptionList.shadowRoot!.querySelector(
        'tab-discard-exception-tabbed-add-dialog');
    assertFalse(!!dialog);
  }

  function assertEditDialogDoesNotExist() {
    const dialog = exceptionList.shadowRoot!.querySelector(
        'tab-discard-exception-edit-dialog');
    assertFalse(!!dialog);
  }

  async function inputDialog(
      dialog: ExceptionAddDialogElement|ExceptionTabbedAddDialogElement|
      ExceptionEditDialogElement,
      input: string) {
    const inputEvent = eventToPromise('input', dialog.$.input.$.input);
    dialog.$.input.$.input.value = input;
    dialog.$.input.$.input.dispatchEvent(new CustomEvent('input'));
    await inputEvent;
    dialog.$.actionButton.click();
  }

  test('testExceptionListAdd', async function() {
    setupExceptionListEntries(['foo']);
    assertAddDialogDoesNotExist();

    exceptionList.$.addButton.click();
    flush();

    const addDialog = await getAddDialog();
    assertTrue(addDialog.$.dialog.open);
    assertEquals('', addDialog.$.input.$.input.value);
    await inputDialog(addDialog, 'bar');
    assertEquals(
        MemorySaverModeExceptionListAction.ADD_MANUAL,
        await performanceMetricsProxy.whenCalled('recordExceptionListAction'));
    assertExceptionListEquals(['foo', 'bar']);
  });

  test('testExceptionListEdit', async function() {
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
        MemorySaverModeExceptionListAction.EDIT,
        await performanceMetricsProxy.whenCalled('recordExceptionListAction'));
    assertExceptionListEquals(['foo', 'baz']);
  });

  test('testExceptionListAddAfterMenuClick', async function() {
    setupExceptionListEntries(['foo']);
    clickMoreActionsButton(getExceptionListEntry(0));
    exceptionList.$.addButton.click();
    flush();

    const addDialog = await getAddDialog();
    assertEquals('', addDialog.$.input.$.input.value);
  });

  test('testExceptionListAddExceptionOverflow', async function() {
    assertTrue(exceptionList.$.expandButton.hidden);

    const entries = [
      ...Array(TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE + 1).keys(),
    ].map(index => `rule${index}`);
    setupExceptionListEntries([...entries]);
    assertFalse(exceptionList.$.collapse.opened);
    assertFalse(exceptionList.$.expandButton.hidden);

    exceptionList.$.expandButton.click();
    await exceptionList.$.expandButton.updateComplete;
    assertTrue(exceptionList.$.collapse.opened);

    exceptionList.$.expandButton.click();
    await exceptionList.$.expandButton.updateComplete;
    assertFalse(exceptionList.$.collapse.opened);

    exceptionList.$.addButton.click();
    flush();

    const newRule = `rule${TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE + 1}`;
    const addDialog = await getAddDialog();
    await inputDialog(addDialog, newRule);
    assertFalse(exceptionList.$.collapse.opened);
    assertExceptionListEquals([...entries, newRule]);
  });

  test('testExceptionListAddExceptionsOverflow', async function() {
    if (!loadTimeData.getBoolean('isDiscardExceptionsImprovementsEnabled')) {
      return;
    }
    const existingEntry = 'www.foo.com';
    setupExceptionListEntries([existingEntry]);
    const entries = [
      ...Array(TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE).keys(),
    ].map(index => `rule${index}`);
    performanceBrowserProxy.setCurrentOpenSites(entries);
    exceptionList.$.addButton.click();
    flush();

    const addDialog = await getTabbedAddDialog();
    await eventToPromise('iron-resize', addDialog);
    flush();

    addDialog.$.list.$.list
        .querySelectorAll<SettingsCheckboxListEntryElement>(
            'settings-checkbox-list-entry:not([hidden])')
        .forEach(currentSitesEntryElement => {
          currentSitesEntryElement.$.checkbox.click();
        });
    assertFalse(addDialog.$.actionButton.disabled);
    addDialog.$.actionButton.click();
    flush();

    assertEquals(false, exceptionList.$.collapse.opened);
    assertExceptionListEquals([existingEntry, ...entries]);
  });

  test('testExceptionListOverflowEdit', async function() {
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

  test('testExceptionListOverflowDelete', function() {
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

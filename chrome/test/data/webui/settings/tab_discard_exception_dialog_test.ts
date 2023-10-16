// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {HighEfficiencyModeExceptionListAction, MAX_TAB_DISCARD_EXCEPTION_RULE_LENGTH, PerformanceBrowserProxyImpl, PerformanceMetricsProxyImpl, SettingsCheckboxListEntryElement, TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE, TAB_DISCARD_EXCEPTIONS_PREF, TabDiscardExceptionAddDialogElement, TabDiscardExceptionAddDialogTabs, TabDiscardExceptionEditDialogElement, TabDiscardExceptionTabbedAddDialogElement} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestPerformanceBrowserProxy} from './test_performance_browser_proxy.js';
import {TestPerformanceMetricsProxy} from './test_performance_metrics_proxy.js';

suite('TabDiscardExceptionsDialog', function() {
  let dialog: TabDiscardExceptionAddDialogElement|
      TabDiscardExceptionTabbedAddDialogElement|
      TabDiscardExceptionEditDialogElement;
  let performanceBrowserProxy: TestPerformanceBrowserProxy;
  let performanceMetricsProxy: TestPerformanceMetricsProxy;

  const EXISTING_RULE = 'foo';
  const INVALID_RULE = 'bar';
  const VALID_RULE = 'baz';

  setup(function() {
    performanceBrowserProxy = new TestPerformanceBrowserProxy();
    performanceBrowserProxy.setValidationResults({
      [EXISTING_RULE]: true,
      [INVALID_RULE]: false,
      [VALID_RULE]: true,
    });
    PerformanceBrowserProxyImpl.setInstance(performanceBrowserProxy);

    performanceMetricsProxy = new TestPerformanceMetricsProxy();
    PerformanceMetricsProxyImpl.setInstance(performanceMetricsProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  function setupDialog(dialog: TabDiscardExceptionAddDialogElement|
                       TabDiscardExceptionTabbedAddDialogElement|
                       TabDiscardExceptionEditDialogElement) {
    dialog.set('prefs', {
      performance_tuning: {
        tab_discarding: {
          exceptions: {
            type: chrome.settingsPrivate.PrefType.LIST,
            value: [EXISTING_RULE],
          },
        },
      },
    });
    document.body.appendChild(dialog);
    flush();
  }

  function setupAddDialog(): TabDiscardExceptionAddDialogElement {
    const addDialog: TabDiscardExceptionAddDialogElement =
        document.createElement('tab-discard-exception-add-dialog');
    setupDialog(addDialog);
    return addDialog;
  }

  async function setupTabbedAddDialog():
      Promise<TabDiscardExceptionTabbedAddDialogElement> {
    const addDialog: TabDiscardExceptionTabbedAddDialogElement =
        document.createElement('tab-discard-exception-tabbed-add-dialog');
    setupDialog(addDialog);
    await performanceBrowserProxy.whenCalled('getCurrentOpenSites');
    performanceBrowserProxy.resetResolver('getCurrentOpenSites');
    return addDialog;
  }

  function setupEditDialog(): TabDiscardExceptionEditDialogElement {
    const editDialog: TabDiscardExceptionEditDialogElement =
        document.createElement('tab-discard-exception-edit-dialog');
    setupDialog(editDialog);
    editDialog.setRuleToEditForTesting(EXISTING_RULE);
    return editDialog;
  }

  async function assertUserInputValidated(rule: string) {
    performanceBrowserProxy.reset();
    const trimmedRule = rule.trim();
    dialog.$.input.$.input.value = rule;
    dialog.$.input.$.input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    if (trimmedRule &&
        trimmedRule.length <= MAX_TAB_DISCARD_EXCEPTION_RULE_LENGTH) {
      const validatedRule = await performanceBrowserProxy.whenCalled(
          'validateTabDiscardExceptionRule');
      assertEquals(trimmedRule, validatedRule);
    }
  }

  async function testValidation() {
    await assertUserInputValidated('   ');
    assertFalse(dialog.$.input.$.input.invalid);
    assertTrue(dialog.$.actionButton.disabled);

    await assertUserInputValidated(
        'a'.repeat(MAX_TAB_DISCARD_EXCEPTION_RULE_LENGTH + 1));
    assertTrue(dialog.$.input.$.input.invalid);
    assertTrue(dialog.$.actionButton.disabled);

    await assertUserInputValidated(VALID_RULE);
    assertFalse(dialog.$.input.$.input.invalid);
    assertFalse(dialog.$.actionButton.disabled);

    await assertUserInputValidated(INVALID_RULE);
    assertTrue(dialog.$.input.$.input.invalid);
    assertTrue(dialog.$.actionButton.disabled);
  }

  test('testTabDiscardExceptionsAddDialogState', async function() {
    dialog = setupAddDialog();
    assertTrue(dialog.$.dialog.open);
    assertFalse(dialog.$.input.$.input.invalid);
    assertTrue(dialog.$.actionButton.disabled);

    await testValidation();
  });

  test('testTabDiscardExceptionsTabbedAddDialogState', async function() {
    dialog = await setupTabbedAddDialog();
    assertTrue(dialog.$.dialog.open);
    assertEquals(
        TabDiscardExceptionAddDialogTabs.MANUAL, dialog.$.tabs.selected);
    assertFalse(dialog.$.input.$.input.invalid);
    assertTrue(dialog.$.actionButton.disabled);

    await testValidation();
  });

  test('testTabDiscardExceptionsListEditDialogState', async function() {
    dialog = setupEditDialog();
    assertTrue(dialog.$.dialog.open);
    assertFalse(dialog.$.input.$.input.invalid);
    assertFalse(dialog.$.actionButton.disabled);

    await testValidation();
  });

  function assertCancel() {
    dialog.$.cancelButton.click();

    assertFalse(dialog.$.dialog.open);
    assertDeepEquals(
        dialog.getPref(TAB_DISCARD_EXCEPTIONS_PREF).value, [EXISTING_RULE]);
  }

  test('testTabDiscardExceptionsAddDialogCancel', async function() {
    dialog = setupAddDialog();
    await assertUserInputValidated(VALID_RULE);
    assertCancel();
  });

  test('testTabDiscardExceptionsTabbedAddDialogCancel', async function() {
    dialog = await setupTabbedAddDialog();
    await assertUserInputValidated(VALID_RULE);
    assertCancel();
  });

  test('testTabDiscardExceptionsEditDialogCancel', async function() {
    dialog = setupEditDialog();
    await assertUserInputValidated(VALID_RULE);
    assertCancel();
  });

  function assertSubmit(expectedRules: string[]) {
    dialog.$.actionButton.click();

    assertFalse(dialog.$.dialog.open);
    assertDeepEquals(
        dialog.getPref(TAB_DISCARD_EXCEPTIONS_PREF).value, expectedRules);
  }

  test('testTabDiscardExceptionsAddDialogSubmit', async function() {
    dialog = setupAddDialog();
    await assertUserInputValidated(VALID_RULE);
    assertSubmit([EXISTING_RULE, VALID_RULE]);
    const action =
        await performanceMetricsProxy.whenCalled('recordExceptionListAction');
    assertEquals(HighEfficiencyModeExceptionListAction.ADD_MANUAL, action);
  });

  test('testTabDiscardExceptionsAddDialogSubmitExisting', async function() {
    dialog = setupAddDialog();
    await assertUserInputValidated(EXISTING_RULE);
    assertSubmit([EXISTING_RULE]);
  });

  test('testTabDiscardExceptionsTabbedAddDialogSubmit', async function() {
    dialog = await setupTabbedAddDialog();
    await assertUserInputValidated(VALID_RULE);
    assertSubmit([EXISTING_RULE, VALID_RULE]);
    const action =
        await performanceMetricsProxy.whenCalled('recordExceptionListAction');
    assertEquals(HighEfficiencyModeExceptionListAction.ADD_MANUAL, action);
  });

  test(
      'testTabDiscardExceptionsTabbedAddDialogSubmitExisting',
      async function() {
        dialog = await setupTabbedAddDialog();
        await assertUserInputValidated(EXISTING_RULE);
        assertSubmit([EXISTING_RULE]);
      });

  test('testTabDiscardExceptionsEditDialogSubmit', async function() {
    dialog = setupEditDialog();
    await assertUserInputValidated(VALID_RULE);
    assertSubmit([VALID_RULE]);
    const action =
        await performanceMetricsProxy.whenCalled('recordExceptionListAction');
    assertEquals(HighEfficiencyModeExceptionListAction.EDIT, action);
  });

  test('testTabDiscardExceptionsEditDialogSubmitExisting', async function() {
    dialog.setPrefValue(
        TAB_DISCARD_EXCEPTIONS_PREF, [EXISTING_RULE, VALID_RULE]);
    dialog = setupEditDialog();
    await assertUserInputValidated(VALID_RULE);
    assertSubmit([VALID_RULE]);
  });

  function assertRulesListEquals(
      dialog: TabDiscardExceptionTabbedAddDialogElement, rules: string[]) {
    const actual = dialog.$.list.$.list.items!;
    assertDeepEquals(rules, actual);
  }

  function getRulesListEntry(
      dialog: TabDiscardExceptionTabbedAddDialogElement,
      idx: number): SettingsCheckboxListEntryElement {
    const entry = [
      ...dialog.$.list.$.list
          .querySelectorAll<SettingsCheckboxListEntryElement>(
              'settings-checkbox-list-entry:not([hidden])'),
    ][idx];
    assertTrue(!!entry);
    return entry;
  }

  test('testTabDiscardExceptionsTabbedAddDialogListEmpty', async function() {
    performanceBrowserProxy.setCurrentOpenSites([EXISTING_RULE]);
    dialog = await setupTabbedAddDialog();

    assertEquals(
        TabDiscardExceptionAddDialogTabs.MANUAL, dialog.$.tabs.selected);
    assertFalse(dialog.$.list.getIsUpdatingForTesting());
  });

  test('testTabDiscardExceptionsTabbedAddDialogList', async function() {
    const expectedRules =
        [...Array(TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE).keys()].map(
            index => `rule${index}`);
    performanceBrowserProxy.setCurrentOpenSites(
        [EXISTING_RULE, ...expectedRules]);
    dialog = await setupTabbedAddDialog();
    await eventToPromise('iron-resize', dialog);
    flush();

    assertEquals(
        TabDiscardExceptionAddDialogTabs.CURRENT_SITES, dialog.$.tabs.selected);
    assertRulesListEquals(dialog, expectedRules);
    assertTrue(dialog.$.actionButton.disabled);
    getRulesListEntry(dialog, 2).click();
    assertFalse(dialog.$.actionButton.disabled);
    getRulesListEntry(dialog, 4).click();
    assertSubmit([EXISTING_RULE, 'rule2', 'rule4']);
  });

  function switchAddDialogTab(
      dialog: TabDiscardExceptionTabbedAddDialogElement,
      tabId: TabDiscardExceptionAddDialogTabs) {
    const tabs =
        dialog.$.tabs.shadowRoot!.querySelectorAll<HTMLElement>('.tab');
    const tab = tabs[tabId];
    assertTrue(!!tab);
    tab.click();
  }

  test('testTabDiscardExceptionsTabbedAddDialogSwitchTabs', async function() {
    performanceBrowserProxy.setCurrentOpenSites([VALID_RULE]);
    dialog = await setupTabbedAddDialog();
    flush();

    getRulesListEntry(dialog, 0).click();
    assertFalse(dialog.$.actionButton.disabled);
    switchAddDialogTab(dialog, TabDiscardExceptionAddDialogTabs.MANUAL);
    assertTrue(dialog.$.actionButton.disabled);
    switchAddDialogTab(dialog, TabDiscardExceptionAddDialogTabs.CURRENT_SITES);
    assertFalse(dialog.$.actionButton.disabled);

    getRulesListEntry(dialog, 0).click();
    switchAddDialogTab(dialog, TabDiscardExceptionAddDialogTabs.MANUAL);
    await assertUserInputValidated(VALID_RULE);
    assertFalse(dialog.$.actionButton.disabled);
    switchAddDialogTab(dialog, TabDiscardExceptionAddDialogTabs.CURRENT_SITES);
    assertTrue(dialog.$.actionButton.disabled);
    switchAddDialogTab(dialog, TabDiscardExceptionAddDialogTabs.MANUAL);
    await performanceBrowserProxy.whenCalled('validateTabDiscardExceptionRule');
    assertFalse(dialog.$.actionButton.disabled);
  });

  test('testTabDiscardExceptionsTabbedAddDialogLiveUpdate', async function() {
    const UPDATE_INTERVAL_MS = 3;
    const INITIAL_SITE = 'siteA';
    const CHANGED_SITE = 'siteB';
    const CHANGED_SITE_SWITCH_TAB = 'siteC';
    const CHANGED_SITE_DOCUMENT_HIDDEN = 'siteD';

    performanceBrowserProxy.setCurrentOpenSites([INITIAL_SITE]);
    dialog = await setupTabbedAddDialog();
    dialog.$.list.setUpdateIntervalForTesting(UPDATE_INTERVAL_MS);
    await eventToPromise('iron-resize', dialog.$.list.$.list);
    flush();

    assertTrue(dialog.$.list.getIsUpdatingForTesting());
    assertRulesListEquals(dialog, [INITIAL_SITE]);
    performanceBrowserProxy.setCurrentOpenSites([CHANGED_SITE]);
    await new Promise((resolve) => setTimeout(resolve, UPDATE_INTERVAL_MS));
    assertRulesListEquals(dialog, [CHANGED_SITE]);

    // after switching to the manual tab, list should no longer update
    switchAddDialogTab(dialog, TabDiscardExceptionAddDialogTabs.MANUAL);
    await performanceBrowserProxy.whenCalled('getCurrentOpenSites');
    assertFalse(dialog.$.list.getIsUpdatingForTesting());
    await new Promise((resolve) => setTimeout(resolve, UPDATE_INTERVAL_MS));
    performanceBrowserProxy.setCurrentOpenSites([CHANGED_SITE_SWITCH_TAB]);
    assertRulesListEquals(dialog, [CHANGED_SITE]);

    // after switching back to the list tab, list should start updating again
    performanceBrowserProxy.resetResolver('getCurrentOpenSites');
    switchAddDialogTab(dialog, TabDiscardExceptionAddDialogTabs.CURRENT_SITES);
    await performanceBrowserProxy.whenCalled('getCurrentOpenSites');
    await eventToPromise('iron-resize', dialog.$.list.$.list);
    assertTrue(dialog.$.list.getIsUpdatingForTesting());
    await new Promise((resolve) => setTimeout(resolve, UPDATE_INTERVAL_MS));
    assertRulesListEquals(dialog, [CHANGED_SITE_SWITCH_TAB]);

    // after document is hidden, list should no longer update
    Object.defineProperty(
        document, 'visibilityState', {value: 'hidden', writable: true});
    performanceBrowserProxy.resetResolver('getCurrentOpenSites');
    document.dispatchEvent(new Event('visibilitychange'));
    await performanceBrowserProxy.whenCalled('getCurrentOpenSites');
    assertFalse(dialog.$.list.getIsUpdatingForTesting());
    await new Promise((resolve) => setTimeout(resolve, UPDATE_INTERVAL_MS));
    performanceBrowserProxy.setCurrentOpenSites([CHANGED_SITE_DOCUMENT_HIDDEN]);
    assertRulesListEquals(dialog, [CHANGED_SITE_SWITCH_TAB]);

    // after document becomes visible, list should start updating again
    Object.defineProperty(
        document, 'visibilityState', {value: 'visible', writable: true});
    performanceBrowserProxy.resetResolver('getCurrentOpenSites');
    document.dispatchEvent(new Event('visibilitychange'));
    await performanceBrowserProxy.whenCalled('getCurrentOpenSites');
    await eventToPromise('iron-resize', dialog.$.list.$.list);
    assertTrue(dialog.$.list.getIsUpdatingForTesting());
    await new Promise((resolve) => setTimeout(resolve, UPDATE_INTERVAL_MS));
    assertRulesListEquals(dialog, [CHANGED_SITE_DOCUMENT_HIDDEN]);
  });
});

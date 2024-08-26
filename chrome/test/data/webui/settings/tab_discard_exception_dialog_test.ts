// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {ExceptionEditDialogElement, ExceptionTabbedAddDialogElement, SettingsCheckboxListEntryElement} from 'chrome://settings/settings.js';
import {convertDateToWindowsEpoch, ExceptionAddDialogTabs, MAX_TAB_DISCARD_EXCEPTION_RULE_LENGTH, MemorySaverModeExceptionListAction, PerformanceBrowserProxyImpl, PerformanceMetricsProxyImpl, TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE, TAB_DISCARD_EXCEPTIONS_PREF} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertLT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPerformanceBrowserProxy} from './test_performance_browser_proxy.js';
import {TestPerformanceMetricsProxy} from './test_performance_metrics_proxy.js';

suite('TabDiscardExceptionsDialog', function() {
  let dialog: ExceptionTabbedAddDialogElement|ExceptionEditDialogElement;
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

  function setupDialog(dialog: ExceptionTabbedAddDialogElement|
                       ExceptionEditDialogElement) {
    dialog.set('prefs', {
      performance_tuning: {
        tab_discarding: {
          exceptions_with_time: {
            type: chrome.settingsPrivate.PrefType.DICTIONARY,
            value: {[EXISTING_RULE]: convertDateToWindowsEpoch()},
          },
        },
      },
    });
    document.body.appendChild(dialog);
    flush();
  }

  async function setupTabbedAddDialog():
      Promise<ExceptionTabbedAddDialogElement> {
    const addDialog: ExceptionTabbedAddDialogElement =
        document.createElement('tab-discard-exception-tabbed-add-dialog');
    setupDialog(addDialog);
    await performanceBrowserProxy.whenCalled('getCurrentOpenSites');
    performanceBrowserProxy.resetResolver('getCurrentOpenSites');
    return addDialog;
  }

  function setupEditDialog(): ExceptionEditDialogElement {
    const editDialog: ExceptionEditDialogElement =
        document.createElement('tab-discard-exception-edit-dialog');
    setupDialog(editDialog);
    editDialog.setRuleToEditForTesting(EXISTING_RULE);
    return editDialog;
  }

  async function assertUserInputValidated(rule: string) {
    performanceBrowserProxy.reset();
    const trimmedRule = rule.trim();
    dialog.$.input.$.input.value = rule;
    await dialog.$.input.$.input.updateComplete;
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

  test('testExceptionTabbedAddDialogState', async function() {
    dialog = await setupTabbedAddDialog();
    assertTrue(dialog.$.dialog.open);
    assertEquals(ExceptionAddDialogTabs.MANUAL, dialog.$.tabs.selected);
    assertFalse(dialog.$.input.$.input.invalid);
    assertTrue(dialog.$.actionButton.disabled);

    await testValidation();
  });

  test('testExceptionListEditDialogState', async function() {
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
        Object.keys(dialog.getPref(TAB_DISCARD_EXCEPTIONS_PREF).value),
        [EXISTING_RULE]);
  }

  test('testExceptionTabbedAddDialogCancel', async function() {
    dialog = await setupTabbedAddDialog();
    await assertUserInputValidated(VALID_RULE);
    assertCancel();
  });

  test('testExceptionEditDialogCancel', async function() {
    dialog = setupEditDialog();
    await assertUserInputValidated(VALID_RULE);
    assertCancel();
  });

  function assertSubmit(expectedRules: string[]) {
    dialog.$.actionButton.click();

    assertFalse(dialog.$.dialog.open);
    assertDeepEquals(
        Object.keys(dialog.getPref(TAB_DISCARD_EXCEPTIONS_PREF).value),
        expectedRules);
  }

  test('testExceptionTabbedAddDialogSubmit', async function() {
    dialog = await setupTabbedAddDialog();
    await assertUserInputValidated(VALID_RULE);
    assertSubmit([EXISTING_RULE, VALID_RULE]);
    const action =
        await performanceMetricsProxy.whenCalled('recordExceptionListAction');
    assertEquals(MemorySaverModeExceptionListAction.ADD_MANUAL, action);
  });

  test('testExceptionTabbedAddDialogSubmitExisting', async function() {
    dialog = await setupTabbedAddDialog();
    await assertUserInputValidated(EXISTING_RULE);
    assertSubmit([EXISTING_RULE]);
  });

  test('testExceptionEditDialogSubmit', async function() {
    dialog = setupEditDialog();
    await assertUserInputValidated(VALID_RULE);
    assertSubmit([VALID_RULE]);
    const action =
        await performanceMetricsProxy.whenCalled('recordExceptionListAction');
    assertEquals(MemorySaverModeExceptionListAction.EDIT, action);
  });

  test('testExceptionEditDialogSubmitExisting', async function() {
    dialog.setPrefValue(TAB_DISCARD_EXCEPTIONS_PREF, {
      EXISTING_RULE: convertDateToWindowsEpoch(),
      VALID_RULE: convertDateToWindowsEpoch(),
    });
    dialog = setupEditDialog();
    await assertUserInputValidated(VALID_RULE);
    assertSubmit([VALID_RULE]);
  });

  function assertRulesListEquals(
      dialog: ExceptionTabbedAddDialogElement, rules: string[]) {
    const actual = dialog.$.list.$.list.items!;
    assertDeepEquals(rules, actual);
  }

  function getRulesListEntry(
      dialog: ExceptionTabbedAddDialogElement,
      idx: number): SettingsCheckboxListEntryElement {
    const entry = [
      ...dialog.$.list.$.list
          .querySelectorAll<SettingsCheckboxListEntryElement>(
              'settings-checkbox-list-entry:not([hidden])'),
    ][idx];
    assertTrue(!!entry);
    return entry;
  }

  test('testExceptionEditDialogUpdateTimestamp', async function() {
    dialog = setupEditDialog();
    await assertUserInputValidated(VALID_RULE);
    assertSubmit([VALID_RULE]);

    const originalTimestamp =
        parseInt(dialog.getPref(TAB_DISCARD_EXCEPTIONS_PREF).value[VALID_RULE]);

    dialog = setupEditDialog();
    await assertUserInputValidated(VALID_RULE);
    assertSubmit([VALID_RULE]);
    const updatedTimestamp =
        parseInt(dialog.getPref(TAB_DISCARD_EXCEPTIONS_PREF).value[VALID_RULE]);

    assertLT(originalTimestamp, updatedTimestamp);
  });

  test('testExceptionTabbedAddDialogListEmpty', async function() {
    performanceBrowserProxy.setCurrentOpenSites([EXISTING_RULE]);
    dialog = await setupTabbedAddDialog();

    assertEquals(ExceptionAddDialogTabs.MANUAL, dialog.$.tabs.selected);
    assertFalse(dialog.$.list.getIsUpdatingForTesting());
  });

  test('testExceptionTabbedAddDialogList', async function() {
    const expectedRules =
        [...Array(TAB_DISCARD_EXCEPTIONS_OVERFLOW_SIZE).keys()].map(
            index => `rule${index}`);
    performanceBrowserProxy.setCurrentOpenSites(
        [EXISTING_RULE, ...expectedRules]);
    dialog = await setupTabbedAddDialog();
    await eventToPromise('iron-resize', dialog);
    flush();

    assertEquals(ExceptionAddDialogTabs.CURRENT_SITES, dialog.$.tabs.selected);
    assertRulesListEquals(dialog, expectedRules);
    assertTrue(dialog.$.actionButton.disabled);
    let checkbox = getRulesListEntry(dialog, 2);
    checkbox.click();
    await checkbox.$.checkbox.updateComplete;

    assertFalse(dialog.$.actionButton.disabled);
    checkbox = getRulesListEntry(dialog, 4);
    checkbox.click();
    await checkbox.$.checkbox.updateComplete;
    assertSubmit([EXISTING_RULE, 'rule2', 'rule4']);
  });

  function switchAddDialogTab(
      dialog: ExceptionTabbedAddDialogElement, tabId: ExceptionAddDialogTabs) {
    const tabs =
        dialog.$.tabs.shadowRoot!.querySelectorAll<HTMLElement>('.tab');
    const tab = tabs[tabId];
    assertTrue(!!tab);
    tab.click();
    return microtasksFinished();
  }

  // Flaky on all OSes. TODO(crbug.com/356848453): Fix and enable the test.
  test.skip('testExceptionTabbedAddDialogSwitchTabs', async function() {
    performanceBrowserProxy.setCurrentOpenSites([VALID_RULE]);
    dialog = await setupTabbedAddDialog();
    flush();
    await microtasksFinished();

    const checkbox = getRulesListEntry(dialog, 0);
    checkbox.click();
    await checkbox.$.checkbox.updateComplete;
    assertFalse(dialog.$.actionButton.disabled);
    await switchAddDialogTab(dialog, ExceptionAddDialogTabs.MANUAL);
    assertTrue(dialog.$.actionButton.disabled);
    await switchAddDialogTab(dialog, ExceptionAddDialogTabs.CURRENT_SITES);
    assertFalse(dialog.$.actionButton.disabled);

    checkbox.click();
    await checkbox.$.checkbox.updateComplete;
    switchAddDialogTab(dialog, ExceptionAddDialogTabs.MANUAL);
    await assertUserInputValidated(VALID_RULE);
    assertFalse(dialog.$.actionButton.disabled);
    await switchAddDialogTab(dialog, ExceptionAddDialogTabs.CURRENT_SITES);
    assertTrue(dialog.$.actionButton.disabled);
    switchAddDialogTab(dialog, ExceptionAddDialogTabs.MANUAL);
    await performanceBrowserProxy.whenCalled('validateTabDiscardExceptionRule');
    assertFalse(dialog.$.actionButton.disabled);
  });

  // Flaky on all OSes. TODO(charlesmeng): Fix and enable the test.
  test.skip('testExceptionTabbedAddDialogLiveUpdate', async function() {
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
    switchAddDialogTab(dialog, ExceptionAddDialogTabs.MANUAL);
    await performanceBrowserProxy.whenCalled('getCurrentOpenSites');
    assertFalse(dialog.$.list.getIsUpdatingForTesting());
    await new Promise((resolve) => setTimeout(resolve, UPDATE_INTERVAL_MS));
    performanceBrowserProxy.setCurrentOpenSites([CHANGED_SITE_SWITCH_TAB]);
    assertRulesListEquals(dialog, [CHANGED_SITE]);

    // after switching back to the list tab, list should start updating again
    performanceBrowserProxy.resetResolver('getCurrentOpenSites');
    switchAddDialogTab(dialog, ExceptionAddDialogTabs.CURRENT_SITES);
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

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {HighEfficiencyModeExceptionListAction, MAX_TAB_DISCARD_EXCEPTION_RULE_LENGTH, PerformanceBrowserProxyImpl, PerformanceMetricsProxyImpl, TAB_DISCARD_EXCEPTIONS_PREF, TabDiscardExceptionAddDialogElement, TabDiscardExceptionEditDialogElement} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestPerformanceBrowserProxy} from './test_performance_browser_proxy.js';
import {TestPerformanceMetricsProxy} from './test_performance_metrics_proxy.js';

suite('TabDiscardExceptionsDialog', function() {
  let dialog: TabDiscardExceptionAddDialogElement|
      TabDiscardExceptionEditDialogElement;
  let performanceBrowserProxy: TestPerformanceBrowserProxy;
  let performanceMetricsProxy: TestPerformanceMetricsProxy;

  const EXISTING_RULE = 'foo';
  const INVALID_RULE = 'bar';
  const VALID_RULE = 'baz';

  setup(function() {
    performanceBrowserProxy = new TestPerformanceBrowserProxy();
    performanceBrowserProxy.setValidationResult(true);
    PerformanceBrowserProxyImpl.setInstance(performanceBrowserProxy);

    performanceMetricsProxy = new TestPerformanceMetricsProxy();
    PerformanceMetricsProxyImpl.setInstance(performanceMetricsProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  function setupDialog() {
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

  function setupAddDialog() {
    dialog = document.createElement('tab-discard-exception-add-dialog');
    setupDialog();
  }

  function setupEditDialog() {
    dialog = document.createElement('tab-discard-exception-edit-dialog');
    dialog.setRuleToEditForTesting(EXISTING_RULE);
    setupDialog();
  }

  async function assertUserInputValidated(rule: string) {
    performanceBrowserProxy.reset();
    const trimmedRule = rule.trim();
    dialog.$.input.value = rule;
    dialog.$.input.dispatchEvent(
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
    assertFalse(
        dialog.$.input.invalid,
        'error mesasge should be hidden on empty input');
    assertTrue(
        dialog.$.actionButton.disabled,
        'submit button should be disabled on empty input');

    await assertUserInputValidated(
        'a'.repeat(MAX_TAB_DISCARD_EXCEPTION_RULE_LENGTH + 1));
    assertTrue(
        dialog.$.input.invalid, 'error mesasge should be shown on long input');
    assertTrue(
        dialog.$.actionButton.disabled,
        'submit button should be disabled on long input');

    await assertUserInputValidated(VALID_RULE);
    assertFalse(
        dialog.$.input.invalid,
        'error mesasge should be hidden on valid input');
    assertFalse(
        dialog.$.actionButton.disabled,
        'submit button should be enabled on valid input');

    performanceBrowserProxy.setValidationResult(false);
    await assertUserInputValidated(INVALID_RULE);
    assertTrue(
        dialog.$.input.invalid,
        'error mesasge should be shown on invalid input');
    assertTrue(
        dialog.$.actionButton.disabled,
        'submit button should be disabled on invalid input');
  }

  test('testTabDiscardExceptionsAddDialogState', async function() {
    setupAddDialog();
    assertTrue(dialog.$.dialog.open, 'dialog should be open initially');
    assertFalse(
        dialog.$.input.invalid, 'error mesasge should be hidden initially');
    assertTrue(
        dialog.$.actionButton.disabled,
        'submit button should be disabled initially');

    await testValidation();
  });

  test('testTabDiscardExceptionsListEditDialogState', async function() {
    setupEditDialog();
    assertTrue(dialog.$.dialog.open, 'dialog should be open initially');
    assertFalse(
        dialog.$.input.invalid, 'error mesasge should be hidden initially');
    assertFalse(
        dialog.$.actionButton.disabled,
        'submit button should be enabled initially');

    await testValidation();
  });

  async function assertCancel() {
    dialog.$.cancelButton.click();
    await flushTasks();

    assertFalse(
        dialog.$.dialog.open, 'dialog should be closed after cancelling');
    assertDeepEquals(
        dialog.getPref(TAB_DISCARD_EXCEPTIONS_PREF).value, [EXISTING_RULE]);
  }

  test('testTabDiscardExceptionsAddDialogCancel', async function() {
    setupAddDialog();
    await assertUserInputValidated(VALID_RULE);
    await assertCancel();
  });

  test('testTabDiscardExceptionsEditDialogCancel', async function() {
    setupEditDialog();
    await assertUserInputValidated(VALID_RULE);
    await assertCancel();
  });

  async function assertSubmit(
      expectedRules: string[], validationResult = true) {
    performanceBrowserProxy.setValidationResult(validationResult);
    dialog.$.actionButton.click();
    await performanceBrowserProxy.whenCalled('validateTabDiscardExceptionRule');
    await flushTasks();

    assertFalse(
        dialog.$.dialog.open, 'dialog should be closed after submitting input');
    assertDeepEquals(
        dialog.getPref(TAB_DISCARD_EXCEPTIONS_PREF).value, expectedRules);
  }

  test('testTabDiscardExceptionsAddDialogSubmit', async function() {
    setupAddDialog();
    await assertUserInputValidated(VALID_RULE);
    await assertSubmit([EXISTING_RULE, VALID_RULE]);
    const action =
        await performanceMetricsProxy.whenCalled('recordExceptionListAction');
    assertEquals(HighEfficiencyModeExceptionListAction.ADD, action);
  });

  test('testTabDiscardExceptionsAddDialogSubmitExisting', async function() {
    setupAddDialog();
    await assertUserInputValidated(EXISTING_RULE);
    await assertSubmit([EXISTING_RULE]);
  });

  test('testTabDiscardExceptionsAddDialogSubmitInvalid', async function() {
    setupAddDialog();
    await assertUserInputValidated(INVALID_RULE);
    dialog.$.actionButton.disabled = false;
    await assertSubmit([EXISTING_RULE], false);
  });

  test('testTabDiscardExceptionsEditDialogSubmit', async function() {
    setupEditDialog();
    await assertUserInputValidated(VALID_RULE);
    await assertSubmit([VALID_RULE]);
    const action =
        await performanceMetricsProxy.whenCalled('recordExceptionListAction');
    assertEquals(HighEfficiencyModeExceptionListAction.EDIT, action);
  });

  test('testTabDiscardExceptionsEditDialogSubmitExisting', async function() {
    dialog.setPrefValue(
        TAB_DISCARD_EXCEPTIONS_PREF, [EXISTING_RULE, VALID_RULE]);
    setupEditDialog();
    await assertUserInputValidated(VALID_RULE);
    await assertSubmit([VALID_RULE]);
  });

  test('testTabDiscardExceptionsEditDialogSubmitInvalid', async function() {
    setupEditDialog();
    await assertUserInputValidated(INVALID_RULE);
    dialog.$.actionButton.disabled = false;
    await assertSubmit([EXISTING_RULE], false);
  });
});

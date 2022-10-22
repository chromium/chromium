// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {MAX_TAB_DISCARD_EXCEPTION_RULE_LENGTH, PerformanceBrowserProxyImpl, SUBMIT_EVENT, TabDiscardExceptionDialogElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestPerformanceBrowserProxy} from './test_performance_browser_proxy.js';

suite('TabDiscardExceptionsDialog', function() {
  let dialog: TabDiscardExceptionDialogElement;
  let performanceBrowserProxy: TestPerformanceBrowserProxy;

  const EXISTING_RULE = 'foo';
  const INVALID_RULE = 'bar';
  const VALID_RULE = 'baz';

  setup(function() {
    performanceBrowserProxy = new TestPerformanceBrowserProxy();
    performanceBrowserProxy.setValidationResult(true);
    PerformanceBrowserProxyImpl.setInstance(performanceBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  function setupAddDialog() {
    dialog = document.createElement('tab-discard-exception-dialog');
    dialog.rule = '';
    document.body.appendChild(dialog);
    flush();
  }

  function setupEditDialog() {
    dialog = document.createElement('tab-discard-exception-dialog');
    dialog.rule = EXISTING_RULE;
    document.body.appendChild(dialog);
    flush();
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
    const listener = () => {
      assertNotReached('cancel button should not fire a submit event');
    };
    dialog.addEventListener(
        SUBMIT_EVENT, listener);

    dialog.$.cancelButton.click();
    await flushTasks();

    assertFalse(
        dialog.$.dialog.open, 'dialog should be closed after submitting input');
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

  async function assertSubmit(rule: string) {
    let submitCount = 0;
    const listener = (e: any) => {
      assertEquals(rule, e.detail);
      submitCount++;
    };
    dialog.addEventListener(
        SUBMIT_EVENT, listener);

    dialog.$.actionButton.click();
    await flushTasks();

    assertFalse(
        dialog.$.dialog.open, 'dialog should be closed after submitting input');
    assertEquals(1, submitCount);
  }

  test('testTabDiscardExceptionsAddDialogSubmit', async function() {
    setupAddDialog();
    await assertUserInputValidated(VALID_RULE);
    await assertSubmit(VALID_RULE);
  });

  test('testTabDiscardExceptionsEditDialogSubmit', async function() {
    setupEditDialog();
    await assertUserInputValidated(VALID_RULE);
    await assertSubmit(VALID_RULE);
  });
});

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://chrome-signin/gaia_action_buttons/gaia_action_buttons.js';

import type {CrButtonElement} from '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import {NativeEventTarget} from '//resources/ash/common/event_target.js';
import type {GaiaActionButtonsElement} from 'chrome://chrome-signin/gaia_action_buttons/gaia_action_buttons.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';


const primaryActionLabel = 'fakePrimaryActionLabel';
const secondaryActionLabel = 'fakeSecondaryActionLabel';

class TestAuthenticator extends NativeEventTarget {
  constructor() {
    super();
  }
}

function assertVisibleButtonWithLabel(button: HTMLElement, label: string) {
  assertFalse(button.hidden);
  assertEquals(label, button.textContent!.trim());
}

suite('GaiaActionButtonsTest', () => {
  let actionButtonsComponent: GaiaActionButtonsElement;
  let testAuthenticator: TestAuthenticator;
  let primaryButton: CrButtonElement;
  let secondaryButton: CrButtonElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    actionButtonsComponent = (document.createElement('gaia-action-buttons'));
    document.body.appendChild(actionButtonsComponent);
    testAuthenticator = new TestAuthenticator();
    actionButtonsComponent.setAuthenticatorForTest(testAuthenticator);
    flush();
    primaryButton =
        actionButtonsComponent.shadowRoot!.querySelector('.action-button')!;
    secondaryButton =
        actionButtonsComponent.shadowRoot!.querySelector('.secondary-button')!;
  });

  test('ButtonLabelsAndVisibility', () => {
    // Buttons are hidden by default.
    assertTrue(primaryButton.hidden);
    assertTrue(secondaryButton.hidden);

    testAuthenticator.dispatchEvent(
        new CustomEvent('setPrimaryActionLabel', {detail: primaryActionLabel}));
    assertVisibleButtonWithLabel(primaryButton, primaryActionLabel);
    assertTrue(secondaryButton.hidden);

    testAuthenticator.dispatchEvent(new CustomEvent(
        'setSecondaryActionLabel', {detail: secondaryActionLabel}));
    assertVisibleButtonWithLabel(primaryButton, primaryActionLabel);
    assertVisibleButtonWithLabel(secondaryButton, secondaryActionLabel);

    // Empty label means that button should be hidden.
    testAuthenticator.dispatchEvent(
        new CustomEvent('setPrimaryActionLabel', {detail: ''}));
    assertTrue(primaryButton.hidden);

    testAuthenticator.dispatchEvent(
        new CustomEvent('setSecondaryActionLabel', {detail: ''}));
    assertTrue(secondaryButton.hidden);
  });

  test('SetActionEnabledEvents', () => {
    // Show both buttons.
    testAuthenticator.dispatchEvent(
        new CustomEvent('setPrimaryActionLabel', {detail: primaryActionLabel}));
    testAuthenticator.dispatchEvent(new CustomEvent(
        'setSecondaryActionLabel', {detail: secondaryActionLabel}));
    assertVisibleButtonWithLabel(primaryButton, primaryActionLabel);
    assertVisibleButtonWithLabel(secondaryButton, secondaryActionLabel);

    // Buttons should be enabled by default.
    assertFalse(primaryButton.disabled);
    assertFalse(secondaryButton.disabled);

    // Send setPrimaryActionEnabled event with 'false' value.
    testAuthenticator.dispatchEvent(
        new CustomEvent('setPrimaryActionEnabled', {detail: false}));
    // Primary button should be disabled.
    assertTrue(primaryButton.disabled);
    assertFalse(secondaryButton.disabled);

    // Send setSecondaryActionEnabled event with 'false' value.
    testAuthenticator.dispatchEvent(
        new CustomEvent('setSecondaryActionEnabled', {detail: false}));
    // Secondary button should be disabled.
    assertTrue(primaryButton.disabled);
    assertTrue(secondaryButton.disabled);

    // Send setAllActionsEnabled event with 'true' value.
    testAuthenticator.dispatchEvent(
        new CustomEvent('setAllActionsEnabled', {detail: true}));
    // Both buttons should be enabled.
    assertFalse(primaryButton.disabled);
    assertFalse(secondaryButton.disabled);
  });
});

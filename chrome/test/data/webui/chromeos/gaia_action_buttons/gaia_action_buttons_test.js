// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';

import {GaiaActionButtonsElement} from 'chrome://chrome-signin/gaia_action_buttons/gaia_action_buttons.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

window.gaia_action_buttons_test = {};
const gaia_action_buttons_test = window.gaia_action_buttons_test;
gaia_action_buttons_test.suiteName = 'GaiaActionButtonsTest';

/** @enum {string} */
gaia_action_buttons_test.TestNames = {
  ButtonLabels: 'Button labels and visibility',
  EnabledEvents: '"set...ActionEnabled" events',
};

const primaryActionLabel = 'fakePrimaryActionLabel';
const secondaryActionLabel = 'fakeSecondaryActionLabel';

class TestAuthenticator extends EventTarget {
  constructor() {
    super();
    /** @type {*} */
    this.webviewMessage = null;
  }

  /** @param {*} payload Payload of the HTML5 message. */
  sendMessageToWebview(payload) {
    this.webviewMessage = payload;
  }
}

/**
 * @param {HTMLElement} button
 * @param {string} label
 */
function assertVisibleButtonWithLabel(button, label) {
  assertFalse(button.hidden);
  assertEquals(label, button.textContent.trim());
}

suite(gaia_action_buttons_test.suiteName, () => {
  /** @type {GaiaActionButtonsElement} */
  let actionButtonsComponent;
  /** @type {TestAuthenticator} */
  let testAuthenticator;
  /** @type {CrButtonElement} */
  let primaryButton;
  /** @type {CrButtonElement} */
  let secondaryButton;

  setup(() => {
    document.body.innerHTML = window.trustedTypes.emptyHTML;
    actionButtonsComponent = /** @type {!GaiaActionButtonsElement} */ (
        document.createElement('gaia-action-buttons'));
    document.body.appendChild(actionButtonsComponent);
    testAuthenticator = new TestAuthenticator();
    actionButtonsComponent.setAuthenticatorForTest(testAuthenticator);
    flush();
    primaryButton = /** @type {!CrButtonElement} */ (
        actionButtonsComponent.shadowRoot.querySelector('.action-button'));
    secondaryButton = /** @type {!CrButtonElement} */ (
        actionButtonsComponent.shadowRoot.querySelector('.secondary-button'));
  });

  test(assert(gaia_action_buttons_test.TestNames.ButtonLabels), () => {
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

  test(assert(gaia_action_buttons_test.TestNames.EnabledEvents), () => {
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

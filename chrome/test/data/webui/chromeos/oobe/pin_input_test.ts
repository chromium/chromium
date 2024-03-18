// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://oobe/components/security_token_pin.js';

import {CrButtonElement} from '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import {SecurityTokenPin} from 'chrome://oobe/components/security_token_pin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertFalse, assertGT, assertLE, assertNotEquals, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';

declare global {
  interface HTMLElementEventMap {
    'completed': CustomEvent;
  }
}

suite('pinInputTest', function() {
  const DEFAULT_PARAMETERS = {
    enableUserInput: true,
    hasError: false,
    formattedError: '',
    formattedAttemptsLeft: '',
  };

  let securityTokenPin: SecurityTokenPin;
  let pinKeyboardContainer: HTMLElement;
  let pinKeyboard: HTMLElement;
  let progressElement: HTMLInputElement;
  let pinInput: HTMLInputElement;
  let inputField: HTMLInputElement;
  let errorContainer: HTMLInputElement;
  let errorElement: HTMLInputElement;
  let submitElement: HTMLInputElement;
  let backElement: HTMLInputElement;

  function ensureExists<T>(arg: T): NonNullable<T> {
    assert(arg);
    return arg;
  }

  setup(() => {
    securityTokenPin =
        ensureExists(document.createElement('security-token-pin'));
    document.body.appendChild(securityTokenPin);
    securityTokenPin.onBeforeShow();

    securityTokenPin.parameters = DEFAULT_PARAMETERS;

    pinKeyboardContainer = ensureExists(
        securityTokenPin.shadowRoot?.querySelector('#pinKeyboardContainer'));
    pinKeyboard = ensureExists(
        securityTokenPin.shadowRoot?.querySelector('#pinKeyboard'));
    progressElement =
        ensureExists(securityTokenPin.shadowRoot?.querySelector('#progress'));
    pinInput = ensureExists(pinKeyboard.shadowRoot?.querySelector('#pinInput'));
    inputField = ensureExists(pinInput.shadowRoot?.querySelector('input'));
    errorContainer = ensureExists(
        securityTokenPin.shadowRoot?.querySelector('#errorContainer'));
    errorElement =
        ensureExists(securityTokenPin.shadowRoot?.querySelector('#error'));
    submitElement =
        ensureExists(securityTokenPin.shadowRoot?.querySelector('#submit'));
    backElement =
        ensureExists(securityTokenPin.shadowRoot?.querySelector('#back'));
  });

  teardown(() => {
    securityTokenPin.remove();
  });

  // Test that the 'completed' event is fired when the user submits the input.
  test('completion events in basic flow', () => {
    const FIRST_PIN = '0123';
    const SECOND_PIN = '987';

    let completedEventDetail: CustomEvent|null = null;
    securityTokenPin.addEventListener('completed', (event: CustomEvent) => {
      assertNotEquals(event.detail, null);
      assertEquals(completedEventDetail, null);
      completedEventDetail = event.detail;
    });
    securityTokenPin.addEventListener('cancel', () => {
      assertNotReached();
    });

    // The user enters some value. No 'completed' event is triggered so far.
    pinInput.value = FIRST_PIN;
    assertEquals(completedEventDetail, null);

    // The user submits the PIN. The 'completed' event has been triggered.
    submitElement.click();
    assertEquals(completedEventDetail, FIRST_PIN);
    completedEventDetail = null;

    // The response arrives, requesting to prompt for the PIN again.
    securityTokenPin.parameters = {
      enableUserInput: true,
      hasError: true,
      formattedError: '',
      formattedAttemptsLeft: '',
    };

    // The user enters some value. No new 'completed' event is triggered so far.
    pinInput.value = SECOND_PIN;
    assertEquals(completedEventDetail, null);

    // The user submits the new PIN. The 'completed' event has been triggered.
    submitElement.click();
    assertEquals(completedEventDetail, SECOND_PIN);
  });

  test('non-digit PIN input validity', () => {
    const NON_DIGIT_PIN = '+Aa';
    pinInput.value = NON_DIGIT_PIN;

    assertEquals(pinInput.value, NON_DIGIT_PIN);
    assertEquals(inputField.value, NON_DIGIT_PIN);
  });

  // Test that the 'cancel' event is fired when the user aborts the dialog.
  test('completion events in cancellation flow', () => {
    let cancelEventCount = 0;
    securityTokenPin.addEventListener('cancel', () => {
      ++cancelEventCount;
    });
    securityTokenPin.addEventListener('completed', () => {
      assertNotReached();
    });

    // The user clicks the 'back' button. The cancel event is triggered.
    backElement.click();
    assertEquals(cancelEventCount, 1);
  });

  // Test that the submit button is only enabled when the input is non-empty.
  test('submit button availability', () => {
    // Initially, the submit button is disabled.
    assertTrue(submitElement.disabled);

    // The user enters a single digit. The submit button is enabled.
    pinInput.value = '1';
    assertFalse(submitElement.disabled);

    // The user clears the input. The submit button is disabled.
    pinInput.value = '';
    assertTrue(submitElement.disabled);
  });

  // Test that the input field is disabled when the final error is displayed and
  // no further user input is expected.
  test('input availability', () => {
    // Initially, the input is enabled.
    assertFalse(inputField.disabled);

    // The user enters and submits a PIN. The response arrives, requesting the
    // PIN again. The input is still enabled.
    pinInput.value = '123';
    submitElement.click();
    securityTokenPin.parameters = {
      enableUserInput: true,
      hasError: true,
      formattedError: '',
      formattedAttemptsLeft: '',
    };
    assertFalse(inputField.disabled);

    // The user enters and submits a PIN again. The response arrives, with a
    // final error. The input is disabled.
    pinInput.value = '456';
    submitElement.click();
    securityTokenPin.parameters = {
      enableUserInput: false,
      hasError: true,
      formattedError: '',
      formattedAttemptsLeft: '',
    };
    assertTrue(inputField.disabled);
  });

  // Test that the input field gets cleared when the user is prompted again.
  test('input cleared on new request', () => {
    const PIN = '123';
    pinInput.value = PIN;
    assertEquals(inputField.value, PIN);

    // The user submits the PIN. The response arrives, requesting the PIN again.
    // The input gets cleared.
    submitElement.click();
    securityTokenPin.parameters = {
      enableUserInput: true,
      hasError: true,
      formattedError: '',
      formattedAttemptsLeft: '',
    };
    assertEquals(pinInput.value, '');
    assertEquals(inputField.value, '');
  });

  // // Test that the input field gets cleared when the request fails with the
  // // final error.
  test('input cleared on final error', () => {
    // The user enters and submits a PIN. The response arrives, requesting the
    // PIN again. The input is cleared.
    const PIN = '123';
    pinInput.value = PIN;
    assertEquals(inputField.value, PIN);

    // The user submits the PIN. The response arrives, reporting a final error
    // and that the user input isn't requested anymore. The input gets cleared.
    submitElement.click();
    securityTokenPin.parameters = {
      enableUserInput: false,
      hasError: true,
      formattedError: '',
      formattedAttemptsLeft: '',
    };
    assertEquals(pinInput.value, '');
    assertEquals(inputField.value, '');
  });

  // Test that the PIN can be entered via the on-screen PIN keypad.
  test('PIN input via keypad', () => {
    const PIN = '13097';

    let completedEventDetail = null;
    securityTokenPin.addEventListener('completed', (event: CustomEvent) => {
      completedEventDetail = event.detail;
    });

    // The user clicks the buttons of the on-screen keypad. The input field is
    // updated accordingly.
    for (const character of PIN) {
      const button: CrButtonElement|undefined|null =
          pinKeyboard.shadowRoot?.querySelector('#digitButton' + character);
      assertTrue(button instanceof CrButtonElement);
      button.click();
    }
    assertEquals(pinInput.value, PIN);
    assertEquals(inputField.value, PIN);

    // The user submits the PIN. The completed event is fired, containing the
    // PIN.
    submitElement.click();
    assertEquals(completedEventDetail, PIN);
  });

  // Test that the asynchronously clicking the PIN keypad buttons still results
  // in a correct PIN.
  test('PIN async input via keypad', async function() {
    const PIN = '123';

    function enterPinAsync() {
      return new Promise<void>((resolve, _reject) => {
        // Click `PIN[0]`, then `PIN[1]`, then `PIN[2]`. Use specific delays
        // that can catch ordering bugs in the tested code (in case it handles
        // clicks in asynchronous tasks without proper sequencing).
        setTimeout(() => {
          const button: CrButtonElement|undefined|null =
              pinKeyboard.shadowRoot?.querySelector('#digitButton' + PIN[1]);
          assertTrue(button instanceof CrButtonElement);
          button.click();
        }, 0);

        const button: CrButtonElement|undefined|null =
            pinKeyboard.shadowRoot?.querySelector('#digitButton' + PIN[0]);
        assertTrue(button instanceof CrButtonElement);
        button.click();

        setTimeout(() => {
          const button: CrButtonElement|undefined|null =
              pinKeyboard.shadowRoot?.querySelector('#digitButton' + PIN[2]);
          assertTrue(button instanceof CrButtonElement);
          button.click();
          resolve();
        }, 0);
      });
    }

    let completedEventDetail = null;
    securityTokenPin.addEventListener('completed', (event: CustomEvent) => {
      completedEventDetail = event.detail;
    });

    // The user clicks the buttons of the on-screen keypad. The input field is
    // updated accordingly.
    await enterPinAsync();
    assertEquals(pinInput.value, PIN);
    assertEquals(inputField.value, PIN);

    // The user submits the PIN. The completed event is fired, containing the
    // PIN.
    submitElement.click();
    assertEquals(completedEventDetail, PIN);
  });

  // Test that the error is displayed only when it's set in the request.
  test('error visibility', () => {
    function getErrorContainerVisibility() {
      assert(errorContainer);
      return getComputedStyle(errorContainer).getPropertyValue('visibility');
    }

    // Initially, no error is shown.
    assertEquals(getErrorContainerVisibility(), 'hidden');
    assertFalse(pinInput.hasAttribute('invalid'));

    // The user submits some PIN, and the error response arrives. The error gets
    // displayed.
    pinInput.value = '123';
    submitElement.click();
    securityTokenPin.parameters = {
      enableUserInput: true,
      hasError: true,
      formattedError: '',
      formattedAttemptsLeft: '',
    };
    assertEquals(getErrorContainerVisibility(), 'visible');
    assertTrue(pinInput.hasAttribute('invalid'));

    // The user modifies the input field. No error is shown.
    pinInput.value = '4';
    assertEquals(getErrorContainerVisibility(), 'hidden');
    assertFalse(pinInput.hasAttribute('invalid'));
  });

  // Test the text of the error label.
  test('label text: invalid PIN', () => {
    securityTokenPin.parameters = {
      enableUserInput: true,
      hasError: true,
      formattedError: 'Invalid PIN.',
      formattedAttemptsLeft: '',
    };
    assertEquals(errorElement.textContent, 'Invalid PIN.');
  });

  // Test the text of the error label when the user input is disabled.
  test('label text: max attempts exceeded', () => {
    securityTokenPin.parameters = {
      enableUserInput: false,
      hasError: true,
      formattedError: 'Maximum allowed attempts exceeded.',
      formattedAttemptsLeft: '',
    };
    assertEquals(
        errorElement.textContent, 'Maximum allowed attempts exceeded.');
  });

  // Test the text of the label when the number of attempts left is given.
  test('label text: attempts number', () => {
    securityTokenPin.parameters = {
      enableUserInput: true,
      hasError: false,
      formattedError: '',
      formattedAttemptsLeft: '3 attempts left',
    };
    assertEquals(errorElement.textContent, '3 attempts left');
  });

  // Test that no scrolling is necessary in order to see all dots after entering
  // a PIN of a typical length.
  test('8-digit PIN fits into input', () => {
    const PIN_LENGTH = 8;
    inputField.value = '0'.repeat(PIN_LENGTH);
    assertGT(inputField.scrollWidth, 0);
    assertLE(inputField.scrollWidth, inputField.clientWidth);
  });

  // Test that the distance between characters (dots) is set in a correct way
  // and doesn't fall back to the default value.
  test('PIN input letter-spacing is correctly set up', () => {
    assertNotEquals(
        getComputedStyle(inputField).getPropertyValue('letter-spacing'),
        'normal');
  });

  // Test that the focus on the input field isn't lost when the PIN is requested
  // again after the failed verification.
  test('focus restores after progress animation', () => {
    // The PIN keyboard is displayed initially.
    assertFalse(pinKeyboardContainer.hidden);
    assertTrue(progressElement.hidden);

    // The PIN keyboard gets focused.
    securityTokenPin.focus();
    assertEquals(securityTokenPin.shadowRoot?.activeElement, pinKeyboard);
    assertEquals(
        (inputField.getRootNode() as Document).activeElement, inputField);

    // The user submits some value while keeping the focus on the input field.
    pinInput.value = '123';
    const enterEvent = new KeyboardEvent(
        'keydown', {key: 'Enter', code: 'Enter', keyCode: 13});
    pinInput.dispatchEvent(enterEvent);

    // The PIN keyboard is replaced by the animation UI.
    assertTrue(pinKeyboardContainer.hidden);
    assertFalse(progressElement.hidden);

    // The response arrives, requesting to prompt for the PIN again.
    securityTokenPin.parameters = {
      enableUserInput: true,
      hasError: true,
      formattedError: '',
      formattedAttemptsLeft: '',
    };
    // The PIN keyboard is shown again, replacing the animation UI.
    assertFalse(pinKeyboardContainer.hidden);
    assertTrue(progressElement.hidden);
    // The focus is on the input field.
    assertEquals(securityTokenPin.shadowRoot?.activeElement, pinKeyboard);
    assertEquals(
        (inputField.getRootNode() as Document).activeElement, inputField);
  });

  // Test that the input field gets focused when the PIN is requested again
  // after the failed verification.
  test('focus set after progress animation', () => {
    // The PIN keyboard is displayed initially.
    assertFalse(pinKeyboardContainer.hidden);
    assertTrue(progressElement.hidden);

    // The user submits some value using the "Submit" UI button.
    pinInput.value = '123';
    submitElement.click();
    // The PIN keyboard is replaced by the animation UI.
    assertTrue(pinKeyboardContainer.hidden);
    assertFalse(progressElement.hidden);

    // The response arrives, requesting to prompt for the PIN again.
    securityTokenPin.parameters = {
      enableUserInput: true,
      hasError: true,
      formattedError: '',
      formattedAttemptsLeft: '',
    };
    // The PIN keyboard is shown again, replacing the animation UI.
    assertFalse(pinKeyboardContainer.hidden);
    assertTrue(progressElement.hidden);
    // The focus is on the input field.
    assertEquals(securityTokenPin.shadowRoot?.activeElement, pinKeyboard);
    assertEquals(
        (inputField.getRootNode() as Document).activeElement, inputField);
  });
});

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrButtonElement} from 'chrome://os-settings/os_settings.js';
import {assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {hasBooleanProperty, hasStringProperty, retry, sleep} from '../utils.js';

enum PinDialogType {
  SETUP,
  AUTOSUBMIT,
}

// The test API for a dialog that asks for a PIN. Used for both the "setup pin"
// dialog and the "pin autosubmit" dialog.
export class PinDialogApi {
  private element: HTMLElement;
  private dialogType: PinDialogType;

  constructor(element: HTMLElement) {
    this.element = element;
    switch (element.tagName) {
      case 'SETTINGS-SETUP-PIN-DIALOG':
        this.dialogType = PinDialogType.SETUP;
        break;
      case 'SETTINGS-PIN-AUTOSUBMIT-DIALOG':
        this.dialogType = PinDialogType.AUTOSUBMIT;
        break;
      default:
        assertNotReached('Invalid pin dialog element');
    }
  }

  private shadowRoot(): ShadowRoot {
    const shadowRoot = this.element.shadowRoot;
    assertTrue(shadowRoot !== null);
    return shadowRoot;
  }

  // Returns the setup-pin-keyboard element (NOT the pin-keyboard element!) of
  // the dialog. This element exists for a SETUP dialog only, not for an
  // AUTOSUBMIT dialog.
  private setupPinKeyboard(): HTMLElement {
    assertTrue(this.dialogType === PinDialogType.SETUP);
    // The ID of the setup-pin-keyboard element is (perhaps confusingly) just
    // `pinKeyboard`.
    const setupPinKeyboard = this.shadowRoot().getElementById('pinKeyboard');
    assertTrue(setupPinKeyboard !== null);
    return setupPinKeyboard;
  }

  private pinKeyboard(): HTMLElement&{value: string} {
    let pinKeyboard = null;
    switch (this.dialogType) {
      case PinDialogType.SETUP: {
        // The PinKeyboard element of a SETUP dialog is nested inside the
        // SetupPinKeyboard element.
        const setupPinKeyboard = this.setupPinKeyboard();
        assertTrue(setupPinKeyboard.shadowRoot !== null);
        pinKeyboard = setupPinKeyboard.shadowRoot.getElementById('pinKeyboard');
        break;
      }
      case PinDialogType.AUTOSUBMIT:
        pinKeyboard = this.shadowRoot().getElementById('pinKeyboard');
        break;
    }
    assertTrue(pinKeyboard !== null);
    assertTrue(hasStringProperty(pinKeyboard, 'value'));
    return pinKeyboard;
  }

  private cancelButton(): HTMLElement {
    const button = this.shadowRoot().querySelector('.cancel-button');
    assertTrue(button instanceof HTMLElement);
    return button;
  }

  private submitButton(): CrButtonElement {
    const button = this.shadowRoot().querySelector('.action-button');
    assertTrue(button instanceof CrButtonElement);
    return button;
  }

  private titleElement(): HTMLElement {
    const title = this.shadowRoot().querySelector('div[slot=title]');
    assertTrue(title instanceof HTMLElement);
    return title;
  }

  // Returns the |problemDiv| element in case of a PIN setup dialog, or the
  // |errorDiv| in case of a PIN autosubmit dialog. Returns |null| if the
  // element does not exist or is invisible.
  private problemErrorDiv(): HTMLElement|null {
    let el = null;
    switch (this.dialogType) {
      case PinDialogType.SETUP: {
        const setupPinKeyboard = this.setupPinKeyboard();
        assertTrue(setupPinKeyboard.shadowRoot !== null);
        el = setupPinKeyboard.shadowRoot.getElementById('problemDiv');
        break;
      }
      case PinDialogType.AUTOSUBMIT: {
        el = this.shadowRoot().querySelector('#errorDiv');
        break;
      }
    }

    if (el === null) {
      return null;
    }

    assertTrue(el instanceof HTMLElement);

    if (window.getComputedStyle(el).visibility !== 'visible') {
      return null;
    }

    return el;
  }

  // Returns the backspace button element of the PIN pad.
  backspaceButton(): HTMLElement&{disabled: boolean} {
    const pinKeyboard = this.pinKeyboard();
    assertTrue(pinKeyboard.shadowRoot !== null);

    const backspaceButton =
        pinKeyboard.shadowRoot.getElementById('backspaceButton');
    assertTrue(backspaceButton !== null);
    assertTrue(hasBooleanProperty(backspaceButton, 'disabled'));

    return backspaceButton;
  }

  async enterPin(pin: string): Promise<void> {
    (await retry(() => this.pinKeyboard())).value = pin;
  }

  async submit(): Promise<void> {
    // This sleep shouldn't be here, but appears to be necessary because PIN
    // dialogs can't immediately submit after their PIN values have changed.
    // Consider removing this check and fixing PIN dialog logic.
    await sleep(2000);
    (await retry(() => this.submitButton())).click();
  }

  canSubmit(): boolean {
    return !this.submitButton().disabled;
  }

  async cancel(): Promise<void> {
    (await retry(() => this.cancelButton())).click();
  }

  // Sends a keyboard event to the input control.
  sendKeyboardEvent(ev: KeyboardEvent) {
    const pinKeyboard = this.pinKeyboard();
    assertTrue(pinKeyboard.shadowRoot !== null);
    const pinInput = pinKeyboard.shadowRoot.getElementById('pinInput');
    assertTrue(pinInput instanceof HTMLElement);
    pinInput.dispatchEvent(ev);
  }

  // Returns the current value of the PIN input field. Throws an assertion
  // error if the pin input field cannot be found.
  pinValue(): string {
    return this.pinKeyboard().value;
  }

  // Returns the current title of the dialog. Throws an assertion error if the
  // title field cannot be found.
  titleText(): string {
    return this.titleElement().innerText;
  }

  // Returns the current text of the "submit" control. Throws an assertion
  // error if the title field cannot be found.
  submitText(): string {
    return this.submitButton().innerText;
  }

  // Returns whether an error is shown.
  hasError(): boolean {
    const pe = this.problemErrorDiv();
    if (pe === null) {
      return false;
    }

    switch (this.dialogType) {
      case PinDialogType.SETUP:
        return pe.classList.contains('error');
      case PinDialogType.AUTOSUBMIT:
        return true;
    }
  }

  // Returns whether a warning is shown. This only applies to a PIN setup
  // dialog and must not be called for a PIN autosubmit dialog.
  hasWarning(): boolean {
    assertTrue(this.dialogType === PinDialogType.SETUP);

    const pe = this.problemErrorDiv();
    return pe !== null && pe.classList.contains('warning');
  }
}

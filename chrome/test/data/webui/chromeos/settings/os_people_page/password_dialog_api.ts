// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrButtonElement, CrDialogElement} from 'chrome://os-settings/os_settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import {SetLocalPasswordInputApi} from './set_local_password_input_api.js';

// The test API for a dialog that setting up new password.
export class PasswordDialogApi {
  private element: HTMLElement;

  constructor(element: HTMLElement) {
    this.element = element;
    assertTrue(this.element.shadowRoot !== null);
  }

  localPasswordInput(): SetLocalPasswordInputApi|null {
    const input = this.shadowRoot().getElementById('setPasswordInput');
    if (input === null) {
      return null;
    }
    assertTrue(input instanceof HTMLElement);
    return new SetLocalPasswordInputApi(input);
  }

  isOpened(): boolean {
    const dialog = this.shadowRoot().getElementById('dialog');
    assertTrue(dialog instanceof CrDialogElement);
    return dialog.open;
  }

  canSubmit(): boolean {
    return !this.submitButton().disabled;
  }

  private shadowRoot(): ShadowRoot {
    const shadowRoot = this.element.shadowRoot;
    assertTrue(shadowRoot !== null);
    return shadowRoot;
  }

  private submitButton(): CrButtonElement {
    const button = this.shadowRoot().getElementById('submitButton');
    assertTrue(button instanceof CrButtonElement);
    return button;
  }
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import {PasswordSettingsApiInterface, PasswordSettingsApiReceiver, PasswordSettingsApiRemote} from '../password_settings_api.test-mojom-webui.js';
import {retry, retryUntilSome} from '../utils.js';

import {PasswordDialogApi} from './password_dialog_api.js';

// The test API for the settings-password-settings element.
export class PasswordSettingsApi implements PasswordSettingsApiInterface {
  private element: HTMLElement;

  constructor(element: HTMLElement) {
    this.element = element;
    assertTrue(this.element.shadowRoot !== null);
  }

  newRemote(): PasswordSettingsApiRemote {
    const receiver = new PasswordSettingsApiReceiver(this);
    return receiver.$.bindNewPipeAndPassRemote();
  }

  async assertCanOpenLocalPasswordDialog(): Promise<void> {
    const passwordDialog = await this.openSetLocalPasswordDialog();
    assertTrue(passwordDialog.isOpened());
    assertTrue(!passwordDialog.canSubmit());
  }

  async assertSubmitButtonEnabledForValidPasswordInput(): Promise<void> {
    const dialog = await retryUntilSome(() => this.setLocalPasswordDialog());
    const input = await retryUntilSome(() => dialog.localPasswordInput());
    await input.enterFirstInput('12345678');
    await input.enterConfirmInput('12345678');
    await input.assertFirstInputInvalid(/*invalid=*/ false);
    await input.assertConfirmInputInvalid(/*invalid=*/ false);
    assertTrue(dialog.canSubmit());
  }

  async assertSubmitButtonDisabledForInvalidPasswordInput(): Promise<void> {
    const dialog = await retryUntilSome(() => this.setLocalPasswordDialog());
    const input = await retryUntilSome(() => dialog.localPasswordInput());
    await input.enterFirstInput('12345678');
    await input.enterConfirmInput('12345679');
    await input.assertFirstInputInvalid(/*invalid=*/ false);
    await input.assertConfirmInputInvalid(/*invalid=*/ true);
    assertTrue(!dialog.canSubmit());
  }

  private shadowRoot(): ShadowRoot {
    const shadowRoot = this.element.shadowRoot;
    assertTrue(shadowRoot !== null);
    return shadowRoot;
  }

  private switchLocalPasswordButton(): HTMLElement {
    const button =
        this.shadowRoot().getElementById('switchLocalPasswordButton');
    assertTrue(button instanceof HTMLElement);
    return button;
  }

  private setLocalPasswordDialog(): PasswordDialogApi|null {
    const dialog = this.shadowRoot().getElementById('setLocalPasswordDialog');
    if (dialog === null) {
      return null;
    }
    assertTrue(dialog instanceof HTMLElement);
    return new PasswordDialogApi(dialog);
  }

  private async openSetLocalPasswordDialog(): Promise<PasswordDialogApi> {
    (await retry(() => this.switchLocalPasswordButton())).click();
    return await retryUntilSome(() => this.setLocalPasswordDialog());
  }
}

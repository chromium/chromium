// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import type {PasswordSettingsApiInterface, PasswordSettingsApiRemote} from '../password_settings_api.test-mojom-webui.js';
import {PasswordSettingsApiReceiver} from '../password_settings_api.test-mojom-webui.js';
import {assertAsync, assertForDuration, retry, retryUntilSome} from '../utils.js';

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

  async setPassword(): Promise<void> {
    const passwordDialog = await this.openSetLocalPasswordDialog();
    const input =
        await retryUntilSome(() => passwordDialog.localPasswordInput());
    await input.enterFirstInput('testpassword');
    await input.enterConfirmInput('testpassword');
    await passwordDialog.submit();

    await assertAsync(() => this.hasPassword());
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

  private getRemoveMenuItems(): NodeListOf<HTMLButtonElement> {
    return this.shadowRoot().querySelectorAll('#moreMenu button');
  }

  private removeButton(): HTMLButtonElement {
    // The remove button is the only button in #moreMenu.
    const buttons = this.getRemoveMenuItems();
    assertTrue(buttons.length <= 1);
    const button = buttons[0];
    assertTrue(button instanceof HTMLButtonElement);
    return button;
  }

  private moreButton(): HTMLButtonElement {
    const button =
        this.shadowRoot().getElementById('moreButton') as HTMLButtonElement;
    assertTrue(button != null);
    return button;
  }

  private hasPassword(): boolean {
    const button =
        this.shadowRoot().getElementById('switchLocalPasswordButton');
    return button === null || !isVisible(button);
  }

  async removePassword(): Promise<void> {
    (await retryUntilSome(() => this.moreButton())).click();
    (await retryUntilSome(() => this.removeButton())).click();
  }

  async assertHasPassword(hasPassword: boolean): Promise<void> {
    await assertAsync(() => this.hasPassword() === hasPassword);
    await assertForDuration(() => this.hasPassword() === hasPassword);
  }

  async assertCanRemovePassword(canRemove: boolean): Promise<void> {
    const buttons = this.getRemoveMenuItems();
    await assertAsync(() => canRemove === (buttons.length > 0));
  }

  async assertCanSwitchToLocalPassword(canSwitch: boolean): Promise<void> {
    const button = this.switchLocalPasswordButton();
    if (button == null) {
      assertFalse(canSwitch);
    }
    await assertAsync(() => canSwitch === isVisible(button));
  }
}

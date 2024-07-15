// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrDialogElement} from 'chrome://os-settings/os_settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import {PasswordSettingsApiInterface, PasswordSettingsApiReceiver, PasswordSettingsApiRemote} from '../password_settings_api.test-mojom-webui.js';
import {assertAsync, retryUntilSome} from '../utils.js';

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

  private setLocalPasswordDialog(): HTMLElement {
    const dialog =
        this.shadowRoot().querySelector('settings-set-local-password-dialog');
    assertTrue(dialog instanceof HTMLElement);
    return dialog;
  }

  private isLocalPasswordDialogOpen(): boolean {
    const dialog = this.setLocalPasswordDialog()
                       .shadowRoot!.querySelector<CrDialogElement>('cr-dialog');
    assertTrue(dialog instanceof CrDialogElement);
    return dialog.open;
  }

  async assertCanOpenLocalPasswordDialog(): Promise<void> {
    (await retryUntilSome(() => this.switchLocalPasswordButton())).click();
    await assertAsync(() => this.isLocalPasswordDialogOpen());
  }
}

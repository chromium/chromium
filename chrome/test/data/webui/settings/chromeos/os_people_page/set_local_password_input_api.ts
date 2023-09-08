// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import {SetLocalPasswordInputApiInterface, SetLocalPasswordInputApiReceiver, SetLocalPasswordInputApiRemote} from '../set_local_password_input_api.test-mojom-webui.js';
import {assertAsync, assertForDuration, retry} from '../utils.js';

// The test API for the settings-password-settings element.
export class SetLocalPasswordInputApi implements
    SetLocalPasswordInputApiInterface {
  private element: HTMLElement;

  public constructor(element: HTMLElement) {
    this.element = element;
    assertTrue(element.tagName === 'SET-LOCAL-PASSWORD-INPUT');
  }

  public newRemote(): SetLocalPasswordInputApiRemote {
    const receiver = new SetLocalPasswordInputApiReceiver(this);
    return receiver.$.bindNewPipeAndPassRemote();
  }

  public async enterFirstInput(value: string): Promise<void> {
    const input = await retry(() => this.firstInput());
    input.focus();
    input.value = value;
  }

  public async enterConfirmInput(value: string): Promise<void> {
    const input = await retry(() => this.confirmInput());
    input.focus();
    input.value = value;
  }

  public async assertFirstInputInvalid(invalid: boolean): Promise<void> {
    const input = await retry(() => this.firstInput());
    const property = () => input.invalid === invalid;
    await assertAsync(property);
    await assertForDuration(property);
  }

  public async assertConfirmInputInvalid(invalid: boolean): Promise<void> {
    const input = await retry(() => this.confirmInput());
    const property = () => input.invalid === invalid;
    await assertAsync(property);
    await assertForDuration(property);
  }

  private shadowRoot(): ShadowRoot {
    const shadowRoot = this.element.shadowRoot;
    assertTrue(shadowRoot !== null);
    return shadowRoot;
  }

  private firstInput(): CrInputElement {
    const el = this.shadowRoot().getElementById('firstInput');
    assertTrue(el instanceof CrInputElement);
    return el;
  }

  private confirmInput(): CrInputElement {
    const el = this.shadowRoot().getElementById('confirmInput');
    assertTrue(el instanceof CrInputElement);
    return el;
  }
}

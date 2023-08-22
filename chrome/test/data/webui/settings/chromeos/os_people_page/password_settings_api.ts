// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import {PasswordSettingsApiInterface, PasswordSettingsApiReceiver, PasswordSettingsApiRemote, PasswordType} from '../password_settings_api.test-mojom-webui.js';
import {assertAsync, assertForDuration, hasBooleanProperty} from '../utils.js';

// The test API for the settings-password-settings element.
export class PasswordSettingsApi implements PasswordSettingsApiInterface {
  private element: HTMLElement;

  constructor(element: HTMLElement) {
    this.element = element;
    assertTrue(this.element.shadowRoot !== null);
  }

  public newRemote(): PasswordSettingsApiRemote {
    const receiver = new PasswordSettingsApiReceiver(this);
    return receiver.$.bindNewPipeAndPassRemote();
  }

  private shadowRoot(): ShadowRoot {
    const shadowRoot = this.element.shadowRoot;
    assertTrue(shadowRoot !== null);
    return shadowRoot;
  }

  private queryGaiaRadio(): {checked: boolean}&HTMLElement {
    const el = this.shadowRoot().querySelector('*[name="gaia"]');
    assertTrue(el instanceof HTMLElement);
    assertTrue(hasBooleanProperty(el, 'checked'));
    return el;
  }

  private queryLocalRadio(): {checked: boolean}&HTMLElement {
    const el = this.shadowRoot().querySelector('*[name="local"]');
    assertTrue(el instanceof HTMLElement);
    assertTrue(hasBooleanProperty(el, 'checked'));
    return el;
  }

  selectedPasswordType(): PasswordType|null {
    const gaiaRadio = this.queryGaiaRadio();
    const localRadio = this.queryLocalRadio();

    assertTrue(
        !(gaiaRadio.checked && localRadio.checked),
        'There must be at most one selected password type');

    if (gaiaRadio.checked) {
      return PasswordType.kGaia;
    }
    if (localRadio.checked) {
      return PasswordType.kLocal;
    }

    return null;
  }

  async assertSelectedPasswordType(passwordType: PasswordType|
                                   null): Promise<void> {
    const isSelected = () => this.selectedPasswordType() === passwordType;
    await assertAsync(isSelected);
    await assertForDuration(isSelected);
  }
}

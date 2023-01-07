// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import {LockScreenSettingsInterface, LockScreenSettingsReceiver, LockScreenSettingsRemote, OSSettingsBrowserProcess, OSSettingsDriverInterface, OSSettingsDriverReceiver} from './test_api.test-mojom-webui.js';
import {assertAsync, assertForDuration, hasBooleanProperty, hasProperty, Lazy, querySelectorShadow, retry, retryUntilSome} from './utils.js';

export class LockScreenSettings implements LockScreenSettingsInterface {
  // Relevant elements are stored as lazy values because element identity might
  // change. For example, the settings page destroys |passwordDialog| after
  // successful authentication and creates a new element if it requires
  // authentication again.
  private lockScreen: Lazy<HTMLElement>;
  private passwordDialog: Lazy<HTMLElement|null>;

  constructor(params: {
    lockScreen: Lazy<HTMLElement>,
    passwordDialog: Lazy<HTMLElement|null>,
  }) {
    this.lockScreen = params.lockScreen;
    assertTrue(this.lockScreen().shadowRoot !== null);
    this.passwordDialog = params.passwordDialog;
  }

  private shadowRoot(): ShadowRoot {
    const lockScreen = this.lockScreen();
    assertTrue(lockScreen.shadowRoot !== null);
    return lockScreen.shadowRoot;
  }

  async assertAuthenticated(isAuthenticated: boolean): Promise<void> {
    const property = () => {
      const dialogExists = this.passwordDialog() !== null;
      return isAuthenticated === !dialogExists;
    };

    await assertAsync(property);
    await assertForDuration(property);
  }

  async authenticate(password: string, shouldSucceed: boolean = true):
      Promise<void> {
    const passwordDialog = await retryUntilSome(this.passwordDialog);
    assertTrue(passwordDialog !== null);
    assertTrue(passwordDialog.shadowRoot !== null);

    const passwordPrompt =
        passwordDialog.shadowRoot.getElementById('passwordPrompt');
    assertTrue(passwordPrompt !== null);
    assertTrue(passwordPrompt.shadowRoot !== null);

    const passwordInput =
        passwordPrompt.shadowRoot.getElementById('passwordInput');
    assertTrue(passwordInput !== null);
    assertTrue(hasProperty(passwordInput, 'value'));

    passwordInput.value = password;

    const confirmButton =
        passwordPrompt.shadowRoot.getElementById('confirmButton');
    assertTrue(confirmButton !== null);
    confirmButton.click();

    if (shouldSucceed) {
      await assertAsync(() => this.passwordDialog() === null);
      return;
    }

    // Assert that an error message shows up eventually.
    await retry(() => {
      assertTrue(passwordInput.shadowRoot !== null);
      const errorDiv = passwordInput.shadowRoot.getElementById('error');
      assertTrue(errorDiv !== null);
      assertTrue(!!errorDiv.innerText);
      assertTrue(window.getComputedStyle(errorDiv).visibility === 'visible');
    });
  }

  async authenticateIncorrectly(password: string): Promise<void> {
    await this.authenticate(password, false);
  }

  private recoveryToggle(): HTMLElement&{checked: boolean}|null {
    const toggle = this.shadowRoot().getElementById('recoveryToggle');
    if (toggle === null) {
      return null;
    }
    assertTrue(hasBooleanProperty(toggle, 'checked'));
    return toggle;
  }

  async assertRecoveryControlVisibility(isVisible: boolean): Promise<void> {
    const property = () => {
      const toggle = this.recoveryToggle();
      if (toggle === null) {
        return !isVisible;
      }
      return toggle.hidden === !isVisible;
    };

    await assertAsync(property);
    await assertForDuration(property);
  }

  async assertRecoveryConfigured(isConfigured: boolean): Promise<void> {
    const property = () => {
      const toggle = this.recoveryToggle();
      if (toggle === null) {
        return false;
      }
      return toggle.checked === isConfigured;
    };

    await assertAsync(property);
    await assertForDuration(property);
  }

  async toggleRecoveryConfiguration(): Promise<void> {
    const toggle = await retryUntilSome(() => this.recoveryToggle());
    assertTrue(toggle !== null);
    const previousChecked = toggle.checked;
    const toggleIsFlipped = () => toggle.checked === previousChecked;

    toggle.click();

    // If the toggle flips immediately, that's OK. Otherwise we need to wait
    // until it flips.
    if (!toggleIsFlipped()) {
      return;
    }

    assertTrue(hasBooleanProperty(toggle, 'disabled') && toggle.disabled);
    // Click again to see whether something weird happens.
    toggle.click();

    await assertAsync(toggleIsFlipped);
  }
}

class OSSettingsDriver implements OSSettingsDriverInterface {
  async goToLockScreenSettings():
      Promise<{lockScreenSettings: LockScreenSettingsRemote}> {
    const privacyPage =
        await retryUntilSome(() => querySelectorShadow(document.body, [
                               'os-settings-ui',
                               'os-settings-main',
                               'os-settings-page',
                               'os-settings-privacy-page',
                             ]));
    assertTrue(privacyPage instanceof HTMLElement);
    assertTrue(privacyPage.shadowRoot !== null);


    // Click on button to go to lock screen settings.
    const trigger =
        privacyPage.shadowRoot.getElementById('lockScreenSubpageTrigger');
    assertTrue(trigger !== null);
    trigger.click();

    const lockScreen: Lazy<HTMLElement> = () => {
      assertTrue(privacyPage.shadowRoot !== null);
      const lockScreen =
          privacyPage.shadowRoot.querySelector('settings-lock-screen');
      assertTrue(lockScreen instanceof HTMLElement);
      return lockScreen;
    };

    const passwordDialog: Lazy<HTMLElement|null> = () => {
      assertTrue(privacyPage.shadowRoot !== null);
      return privacyPage.shadowRoot.getElementById('passwordDialog');
    };

    const lockScreenSettings =
        new LockScreenSettings({lockScreen, passwordDialog});
    const receiver = new LockScreenSettingsReceiver(lockScreenSettings);
    const remote = receiver.$.bindNewPipeAndPassRemote();

    return {lockScreenSettings: remote};
  }
}

// Passes an OSSettingsDriver remote to the browser process.
export async function register(): Promise<void> {
  const browserProcess = OSSettingsBrowserProcess.getRemote();
  const receiver = new OSSettingsDriverReceiver(new OSSettingsDriver());
  const remote = receiver.$.bindNewPipeAndPassRemote();
  await browserProcess.registerOSSettingsDriver(remote);
}

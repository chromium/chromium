// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import {LockScreenSettings_RecoveryDialogAction as RecoveryDialogAction, LockScreenSettingsInterface, LockScreenSettingsReceiver, LockScreenSettingsRemote, OSSettingsBrowserProcess, OSSettingsDriverInterface, OSSettingsDriverReceiver} from './test_api.test-mojom-webui.js';
import {assertAsync, assertForDuration, hasBooleanProperty, hasProperty, hasStringProperty, Lazy, querySelectorShadow, retry, retryUntilSome, sleep} from './utils.js';

enum PinDialogType {
  SETUP,
  AUTOSUBMIT,
}

// A dialog that asks for a pin. Used for both the "setup pin" dialog and the
// "pin autosubmit" dialog.
class PinDialog {
  private element: HTMLElement;
  private dialogType: PinDialogType;

  constructor(element: HTMLElement, dialogType: PinDialogType) {
    this.element = element;
    assertTrue(this.element.shadowRoot !== null);
    this.dialogType = dialogType;
  }

  private shadowRoot(): ShadowRoot {
    const shadowRoot = this.element.shadowRoot;
    assertTrue(shadowRoot !== null);
    return shadowRoot;
  }

  private pinInput(): HTMLElement&{value: string} {
    const pinKeyboard = this.shadowRoot().getElementById('pinKeyboard');
    assertTrue(pinKeyboard instanceof HTMLElement);
    assertTrue(pinKeyboard.shadowRoot !== null);

    switch (this.dialogType) {
      case PinDialogType.SETUP: {
        const pinInput = pinKeyboard.shadowRoot.getElementById('pinKeyboard');
        assertTrue(pinInput instanceof HTMLElement);
        assertTrue(hasStringProperty(pinInput, 'value'));
        return pinInput;
      }
      case PinDialogType.AUTOSUBMIT: {
        assertTrue(hasStringProperty(pinKeyboard, 'value'));
        return pinKeyboard;
      }
    }
  }

  private cancelButton(): HTMLElement {
    const button = this.shadowRoot().querySelector('.cancel-button');
    assertTrue(button instanceof HTMLElement);
    return button;
  }

  private submitButton(): HTMLElement {
    const button = this.shadowRoot().querySelector('.action-button');
    assertTrue(button instanceof HTMLElement);
    return button;
  }

  async enterPin(pin: string): Promise<void> {
    (await retry(() => this.pinInput())).value = pin;
  }

  async submit(): Promise<void> {
    await sleep(2000);
    (await retry(() => this.submitButton())).click();
  }

  async cancel(): Promise<void> {
    (await retry(() => this.cancelButton())).click();
  }
}

class RecoveryDialog {
  private element: HTMLElement;

  constructor(element: HTMLElement) {
    this.element = element;
    assertTrue(this.element.shadowRoot !== null);
  }

  private shadowRoot(): ShadowRoot {
    const shadowRoot = this.element.shadowRoot;
    assertTrue(shadowRoot !== null);
    return shadowRoot;
  }

  private cancelButton(): HTMLElement {
    const cancelButton =
        this.shadowRoot().getElementById('cancelRecoveryDialogButton');
    assertTrue(cancelButton !== null);
    assertTrue(cancelButton instanceof HTMLElement);
    return cancelButton;
  }

  private disableButton(): HTMLElement {
    const disableButton =
        this.shadowRoot().getElementById('disableRecoveryDialogButton');
    assertTrue(disableButton !== null);
    assertTrue(disableButton instanceof HTMLElement);
    return disableButton;
  }

  async clickCancel(): Promise<void> {
    (await retry(() => this.cancelButton())).click();
  }

  async clickDisable(): Promise<void> {
    (await retry(() => this.disableButton())).click();
  }
}

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

  private recoveryDisableDialog(): RecoveryDialog|null {
    const element = this.shadowRoot().getElementById('localDataRecoveryDialog');
    if (element === null) {
      return null;
    }
    assertTrue(element instanceof HTMLElement);
    return new RecoveryDialog(element);
  }

  async enableRecoveryConfiguration(): Promise<void> {
    const toggle = await retryUntilSome(() => this.recoveryToggle());
    assertTrue(!toggle.checked);
    toggle.click();

    // If the toggle flips immediately, that's OK. Otherwise we need to wait
    // until it flips.
    if (toggle.checked) {
      return;
    }
    assertTrue(hasBooleanProperty(toggle, 'disabled') && toggle.disabled);
    // Click again to see whether something weird happens.
    toggle.click();
    await assertAsync(() => toggle.checked);
  }

  async tryEnableRecoveryConfiguration(): Promise<void> {
    const toggle = await retryUntilSome(() => this.recoveryToggle());
    assertTrue(!toggle.checked);
    toggle.click();
  }

  async disableRecoveryConfiguration(dialogAction: RecoveryDialogAction):
      Promise<void> {
    assertTrue(this.recoveryDisableDialog() === null);
    const toggle = await retryUntilSome(() => this.recoveryToggle());
    assertTrue(toggle !== null);
    assertTrue(toggle.checked);
    toggle.click();
    // After click on the toggle, the toggle has to be disabled.
    assertTrue(hasBooleanProperty(toggle, 'disabled') && toggle.disabled);
    // RecoveryDialog has to be visible.
    const recoveryDialog =
        await retryUntilSome(() => this.recoveryDisableDialog());
    switch (dialogAction) {
      case RecoveryDialogAction.CancelDialog:
        recoveryDialog.clickCancel();
        await assertAsync(() => toggle.checked);
        break;
      case RecoveryDialogAction.ConfirmDisabling:
        recoveryDialog.clickDisable();
        await assertAsync(() => !toggle.checked);
        break;
      default:
        assertTrue(false);
    }
    await assertAsync(() => this.recoveryDisableDialog() === null);
  }

  private passwordOnlyToggle(): HTMLElement&{checked: boolean} {
    const toggle =
        this.shadowRoot().querySelector('cr-radio-button[name="password"]');
    assertTrue(toggle instanceof HTMLElement);
    assertTrue(hasBooleanProperty(toggle, 'checked'));
    return toggle;
  }

  private pinAndPasswordToggle(): HTMLElement&{checked: boolean}|null {
    const toggle =
        this.shadowRoot().querySelector('cr-radio-button[name="pin+password"]');
    assertTrue(toggle instanceof HTMLElement);
    assertTrue(hasBooleanProperty(toggle, 'checked'));
    return toggle;
  }

  private setupPinButton(): HTMLElement|null {
    return this.shadowRoot().getElementById('setupPinButton');
  }

  private setupPinDialog(): PinDialog|null {
    const element = this.shadowRoot().getElementById('setupPin');
    if (element === null) {
      return null;
    }
    assertTrue(element instanceof HTMLElement);
    return new PinDialog(element, PinDialogType.SETUP);
  }

  private pinAutosubmitDialog(): PinDialog|null {
    const element = this.shadowRoot().getElementById('pinAutosubmitDialog');
    if (element === null) {
      return null;
    }
    assertTrue(element instanceof HTMLElement);
    return new PinDialog(element, PinDialogType.AUTOSUBMIT);
  }

  async assertIsUsingPin(isUsing: boolean): Promise<void> {
    const property = () => {
      const toggle = this.pinAndPasswordToggle();
      return toggle !== null && toggle.checked === isUsing;
    };
    await assertAsync(property);
    await assertForDuration(property);
  }

  async removePin(): Promise<void> {
    (await retry(() => this.passwordOnlyToggle())).click();
    await assertAsync(() => this.passwordOnlyToggle().checked === true);
  }

  async setPin(pin: string): Promise<void> {
    // Click the "pin and password" toggle button.
    (await retryUntilSome(() => this.pinAndPasswordToggle())).click();
    // The toggle button should be checked.
    await assertAsync(() => {
      const toggle = this.pinAndPasswordToggle();
      return toggle !== null && toggle.checked;
    });

    // Click the pin setup button.
    (await retryUntilSome(() => this.setupPinButton())).click();
    // The pin dialog should be shown.
    const pinDialog = await retryUntilSome(() => this.setupPinDialog());

    // Enter pin twice and submit each time.
    await pinDialog.enterPin(pin);
    await pinDialog.submit();
    await pinDialog.enterPin(pin);
    await pinDialog.submit();

    // The setup pin dialog should disappear.
    await assertAsync(() => this.setupPinDialog() === null);

    // The "pin or password" toggle should still be checked.
    await assertAsync(() => {
      const toggle = this.pinAndPasswordToggle();
      return toggle !== null && toggle.checked;
    });
  }

  private autosubmitToggle(): HTMLElement&{checked: boolean}|null {
    const toggle = this.shadowRoot().getElementById('enablePinAutoSubmit');
    if (toggle === null) {
      return null;
    }

    assertTrue(toggle instanceof HTMLElement);
    assertTrue(hasBooleanProperty(toggle, 'checked'));
    return toggle;
  }

  isPinAutosubmitEnabled(): boolean {
    const toggle = this.autosubmitToggle();
    return toggle !== null && toggle.checked;
  }

  async assertPinAutosubmitEnabled(isEnabled: boolean): Promise<void> {
    const check = () => this.isPinAutosubmitEnabled() === isEnabled;
    await assertAsync(check);
    await assertForDuration(check);
  }

  async enablePinAutosubmit(pin: string): Promise<void> {
    // Initially, autosubmit must be disabled.
    await assertAsync(() => this.isPinAutosubmitEnabled() === false);

    // Click the toggle.
    (await retryUntilSome(() => this.autosubmitToggle())).click();

    // Wait for the confirmation dialog to appear and enter pin.
    const dialog = await retryUntilSome(() => this.pinAutosubmitDialog());
    await dialog.enterPin(pin);
    await dialog.submit();

    // The dialog should disappear, and the toggle must be checked.
    await assertAsync(() => this.pinAutosubmitDialog() === null);
    await assertAsync(() => this.isPinAutosubmitEnabled() === true);
  }

  async enablePinAutosubmitIncorrectly(incorrectPin: string): Promise<void> {
    // Initially, autosubmit must be disabled.
    await assertAsync(() => this.isPinAutosubmitEnabled() === false);

    // Click the toggle.
    (await retryUntilSome(() => this.autosubmitToggle())).click();

    // Wait for the confirmation dialog to appear and enter pin.
    const dialog = await retryUntilSome(() => this.pinAutosubmitDialog());
    await dialog.enterPin(incorrectPin);
    await dialog.submit();

    // The dialog should not disappear. Dismiss it.
    await assertForDuration(() => this.pinAutosubmitDialog() !== null);
    await dialog.cancel();
    await assertAsync(() => this.pinAutosubmitDialog() === null);
  }

  async disablePinAutosubmit(): Promise<void> {
    await assertAsync(() => this.isPinAutosubmitEnabled() === true);
    (await retryUntilSome(() => this.autosubmitToggle())).click();
    await assertAsync(() => this.isPinAutosubmitEnabled() === false);
  }
}

class OsSettingsDriver implements OSSettingsDriverInterface {
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

// Passes an OsSettingsDriver remote to the browser process.
export async function register(): Promise<void> {
  const browserProcess = OSSettingsBrowserProcess.getRemote();
  const receiver = new OSSettingsDriverReceiver(new OsSettingsDriver());
  const remote = receiver.$.bindNewPipeAndPassRemote();
  await browserProcess.registerOSSettingsDriver(remote);
}

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';
import 'chrome://os-settings/lazy_load.js';

import {SettingsRadioGroupElement} from 'chrome://os-settings/lazy_load.js';
import {CrButtonElement, SettingsGoogleDriveSubpageElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import {GoogleDriveSettingsInterface, GoogleDriveSettingsReceiver, GoogleDriveSettingsRemote, LockScreenSettings_RecoveryDialogAction as RecoveryDialogAction, LockScreenSettingsInterface, LockScreenSettingsReceiver, LockScreenSettingsRemote, OSSettingsBrowserProcess, OSSettingsDriverInterface, OSSettingsDriverReceiver} from './test_api.test-mojom-webui.js';
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

  private pinKeyboard(): HTMLElement {
    const pinKeyboard = this.shadowRoot().getElementById('pinKeyboard');
    assertTrue(pinKeyboard instanceof HTMLElement);
    return pinKeyboard;
  }

  private pinInput(): HTMLElement&{value: string} {
    const pinKeyboard = this.pinKeyboard();
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
        const pinKeyboard = this.pinKeyboard();
        assertTrue(pinKeyboard.shadowRoot !== null);
        el = pinKeyboard.shadowRoot.getElementById('problemDiv');
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
  backspaceButton(): CrButtonElement {
    const pinKeyboard = this.pinKeyboard();
    assertTrue(pinKeyboard.shadowRoot !== null);

    const backspaceButton =
        pinKeyboard.shadowRoot.getElementById('backspaceButton');
    assertTrue(backspaceButton instanceof CrButtonElement);
    return backspaceButton;
  }

  async enterPin(pin: string): Promise<void> {
    (await retry(() => this.pinInput())).value = pin;
  }

  async submit(): Promise<void> {
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
    this.pinInput().dispatchEvent(ev);
  }

  // Returns the current value of the PIN input field. Throws an assertion
  // error if the pin input field cannot be found.
  pinValue(): string {
    return this.pinInput().value;
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

  async assertRecoveryControlAvailability(isAvailable: boolean): Promise<void> {
    const property = () => {
      const toggle = this.recoveryToggle();
      if (toggle === null) {
        return !isAvailable;
      }
      return toggle.outerHTML.includes('not supported') === !isAvailable;
    };

    await assertAsync(property);
    await assertForDuration(property);
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

  async tryDisableRecoveryConfiguration(): Promise<void> {
    const toggle = await retryUntilSome(() => this.recoveryToggle());
    assertTrue(toggle.checked);
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

  // Selects the "PIN and password" option. This doesn't open the PIN setup
  // dialog, but it should make the "setup PIN" button appear.
  async selectPinAndPassword(): Promise<void> {
    (await retryUntilSome(() => this.pinAndPasswordToggle())).click();
    // The toggle button should be checked.
    await assertAsync(() => {
      const toggle = this.pinAndPasswordToggle();
      return toggle !== null && toggle.checked;
    });
  }

  // Selects the "PIN and password" option and then opens the PIN setup dialog.
  private async openPinSetupDialog(): Promise<PinDialog> {
    await this.selectPinAndPassword();
    (await retryUntilSome(() => this.setupPinButton())).click();
    return await retryUntilSome(() => this.setupPinDialog());
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
    const pinDialog = await this.openPinSetupDialog();

    // Enter pin twice and submit each time.
    const initialTitleText = pinDialog.titleText();
    const initialSubmitText = pinDialog.submitText();

    assertAsync(() => pinDialog.backspaceButton().disabled);
    await pinDialog.enterPin(pin);
    assertAsync(() => !pinDialog.backspaceButton().disabled);

    await pinDialog.submit();

    assertAsync(() => pinDialog.pinValue() === '');
    assertAsync(() => initialTitleText !== pinDialog.titleText());
    assertAsync(() => initialSubmitText !== pinDialog.submitText());

    await pinDialog.enterPin(pin);
    await pinDialog.submit();

    // If the pin setup dialog does not disappear immediately, then at least
    // submitting again should be impossible.
    if (this.setupPinDialog() !== null) {
      assertTrue(!pinDialog.canSubmit());
    }

    // The setup pin dialog should disappear.
    await assertAsync(() => this.setupPinDialog() === null);

    // The "pin or password" toggle should still be checked.
    await assertAsync(() => {
      const toggle = this.pinAndPasswordToggle();
      return toggle !== null && toggle.checked;
    });
  }

  async setPinButCancelConfirmation(pin: string): Promise<void> {
    const pinDialog = await this.openPinSetupDialog();

    await pinDialog.enterPin(pin);
    await pinDialog.submit();
    await pinDialog.cancel();

    // The setup pin dialog should disappear.
    await assertAsync(() => this.setupPinDialog() === null);
  }

  async setPinButFailConfirmation(firstPin: string, secondPin: string):
      Promise<void> {
    const pinDialog = await this.openPinSetupDialog();

    // Enter pin values.
    await pinDialog.enterPin(firstPin);
    await pinDialog.submit();
    await pinDialog.enterPin(secondPin);
    await pinDialog.submit();

    // Assert that the pin dialog shows an error and doesn't allow to submit.
    await assertAsync(() => pinDialog.hasError());
    await assertAsync(() => !pinDialog.canSubmit());

    // Entering a different PIN should make the error disappear and allow to
    // submit again.
    await pinDialog.enterPin(firstPin);
    await assertAsync(() => !pinDialog.hasError());
    await assertAsync(() => pinDialog.canSubmit());

    // Close the dialog.
    await pinDialog.cancel();
    await assertAsync(() => this.setupPinDialog() === null);
  }

  async setPinButTooShort(shortPin: string, okPin: string): Promise<void> {
    const pinDialog = await this.openPinSetupDialog();

    await pinDialog.enterPin(shortPin);
    // The PIN length check currently happens asynchronously, so it always
    // takes a bit until the UI disables or enables submission. This is
    // probably something we want to change, since it allows users to submit
    // PINs that do not satisfy the requirements if they press the "submit"
    // button quickly enough.
    await assertAsync(() => !pinDialog.canSubmit() && !pinDialog.hasError());

    await pinDialog.enterPin(okPin);
    await assertAsync(() => pinDialog.canSubmit() && !pinDialog.hasError());

    await pinDialog.enterPin(shortPin);
    await assertAsync(() => !pinDialog.canSubmit() && pinDialog.hasError());

    await pinDialog.cancel();
    await assertAsync(() => this.setupPinDialog() === null);
  }

  async setPinButTooLong(longPin: string, okPin: string): Promise<void> {
    const pinDialog = await this.openPinSetupDialog();

    await pinDialog.enterPin(okPin);
    await assertAsync(() => pinDialog.canSubmit() && !pinDialog.hasError());

    await pinDialog.enterPin(longPin);
    // The PIN length check happens asynchronously at the moment -- see comment
    // in |setPinButTooShort|.
    await assertAsync(() => !pinDialog.canSubmit() && pinDialog.hasError());

    await pinDialog.cancel();
    await assertAsync(() => this.setupPinDialog() === null);
  }

  async setPinWithWarning(weakPin: string): Promise<void> {
    const pinDialog = await this.openPinSetupDialog();

    await pinDialog.enterPin(weakPin);
    await assertAsync(() => pinDialog.canSubmit() && pinDialog.hasWarning());

    await pinDialog.submit();
    await pinDialog.enterPin(weakPin);
    await pinDialog.submit();

    await assertAsync(() => this.setupPinDialog() === null);
  }

  async checkPinSetupDialogKeyInput(): Promise<void> {
    const pinDialog = await this.openPinSetupDialog();

    // Text input should be blocked.
    pinDialog.sendKeyboardEvent(
        new KeyboardEvent('keydown', {cancelable: true, key: 'a'}));
    assertForDuration(() => pinDialog.pinValue() === '');

    // Numerical input should be allowed.
    pinDialog.sendKeyboardEvent(
        new KeyboardEvent('keydown', {cancelable: true, key: '1'}));
    assertAsync(() => pinDialog.pinValue() === '1');
    await pinDialog.enterPin('');

    // System keys should not be suppressed, but should not affect the PIN
    // value.
    const systemKeyEvent =
        new KeyboardEvent('keydown', {cancelable: true, key: 'BrightnessUp'});
    pinDialog.sendKeyboardEvent(systemKeyEvent);
    assertTrue(!systemKeyEvent.defaultPrevented);
    assertForDuration(() => pinDialog.pinValue() === '');
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

  private isPinAutosubmitEnabled(): boolean {
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
    await assertAsync(() => dialog.hasError());
    await assertForDuration(() => this.pinAutosubmitDialog() !== null);

    await dialog.cancel();
    await assertAsync(() => this.pinAutosubmitDialog() === null);
  }

  async tryEnablePinAutosubmitWithLockedPin(pin: string, password: string):
      Promise<void> {
    await assertAsync(() => this.isPinAutosubmitEnabled() === false);

    (await retryUntilSome(() => this.autosubmitToggle())).click();
    const dialog = await retryUntilSome(() => this.pinAutosubmitDialog());
    await dialog.enterPin(pin);
    await dialog.submit();

    await this.assertAuthenticated(false);
    await this.authenticate(password);

    // The autosubmit dialog should have disappeared, and autosubmit should not
    // have been enabled.
    await assertAsync(
        () => this.pinAutosubmitDialog() === null &&
            !this.isPinAutosubmitEnabled());
  }

  async enablePinAutosubmitTooLong(longPin: string): Promise<void> {
    // Initially, autosubmit must be disabled.
    await assertAsync(() => this.isPinAutosubmitEnabled() === false);

    // Click the toggle.
    (await retryUntilSome(() => this.autosubmitToggle())).click();

    // Wait for the confirmation dialog to appear and enter the PIN.
    const dialog = await retryUntilSome(() => this.pinAutosubmitDialog());
    await dialog.enterPin(longPin);
    // The dialog shouldn't allow submitting |longPin| and synchronously
    // disable the input field.
    assertTrue(!dialog.canSubmit());
    // Eventually an error should appear because the PIN is too long.
    await assertAsync(() => dialog.hasError());

    await dialog.cancel();
    await assertAsync(() => this.pinAutosubmitDialog() === null);
  }

  async disablePinAutosubmit(): Promise<void> {
    await assertAsync(() => this.isPinAutosubmitEnabled() === true);
    (await retryUntilSome(() => this.autosubmitToggle())).click();
    await assertAsync(() => this.isPinAutosubmitEnabled() === false);
  }

  private queryAutoLockScreenToggle(): SettingsToggleButtonElement {
    const toggle = this.shadowRoot().getElementById('enableLockScreen');
    assertTrue(toggle instanceof SettingsToggleButtonElement);
    return toggle;
  }

  private queryLockScreenNotificationSettings(): SettingsRadioGroupElement {
    const notificationSettings =
        this.shadowRoot().getElementById('notificationSettings');
    assertTrue(notificationSettings instanceof SettingsRadioGroupElement);
    return notificationSettings;
  }

  async assertAutoLockScreenEnabled(isEnabled: boolean): Promise<void> {
    const isAutoLockScreenEnabled = () => {
      const toggle = this.queryAutoLockScreenToggle();
      return toggle.checked === isEnabled;
    };

    await assertAsync(isAutoLockScreenEnabled);
    await assertForDuration(isAutoLockScreenEnabled);
  }

  async enableAutoLockScreen(): Promise<void> {
    const toggle = await retryUntilSome(() => this.queryAutoLockScreenToggle());
    await assertAsync(() => !toggle.checked);
    toggle.click();
    await assertAsync(() => toggle.checked);
  }

  async disableAutoLockScreen(): Promise<void> {
    const toggle = await retryUntilSome(() => this.queryAutoLockScreenToggle());
    await assertAsync(() => toggle.checked);
    toggle.click();
    await assertAsync(() => !toggle.checked);
  }

  async assertAutoLockScreenFocused(): Promise<void> {
    const isFocused = () =>
        this.shadowRoot().activeElement === this.queryAutoLockScreenToggle();
    assertAsync(isFocused);
    assertForDuration(isFocused);
  }

  async assertLockScreenNotificationFocused(): Promise<void> {
    const isFocused = () => this.queryLockScreenNotificationSettings().contains(
        this.shadowRoot().activeElement);
    await assertAsync(isFocused);
    await assertForDuration(isFocused);
  }
}

// Page object that implements the Mojo remote to interact with the Google drive
// subpage.
export class GoogleDriveSettings implements GoogleDriveSettingsInterface {
  constructor(private googleDriveSubpage_: SettingsGoogleDriveSubpageElement) {}

  // Ensure the string supplied matched the value that are stored on the google
  // drive subpage element.
  assertRequiredSpace(requiredSpace: string) {
    assertTrue(this.googleDriveSubpage_?.requiredSpace === requiredSpace);
  }

  assertRemainingSpace(remainingSpace: string) {
    assertTrue(this.googleDriveSubpage_?.remainingSpace === remainingSpace);
  }

  async assertBulkPinningSpace(requiredSpace: string, remainingSpace: string):
      Promise<void> {
    this.assertRequiredSpace(requiredSpace);
    this.assertRemainingSpace(remainingSpace);
  }

  async assertBulkPinningPinnedSize(expectedPinnedSize: string): Promise<void> {
    assertTrue(
        this.googleDriveSubpage_?.totalPinnedSize === expectedPinnedSize);
  }

  async clickClearOfflineFilesAndAssertNewSize(newSize: string): Promise<void> {
    const offlineStorageButton =
        this.googleDriveSubpage_.shadowRoot!.querySelector<CrButtonElement>(
            '#drive-offline-storage-row cr-button')!;
    offlineStorageButton.click();

    // Click the confirm button on the confirmation dialog.
    const getConfirmationButton = () =>
        querySelectorShadow(
            this.googleDriveSubpage_.shadowRoot!,
            [
              'settings-drive-confirmation-dialog',
              '.action-button',
            ])! as CrButtonElement |
        null;
    await assertAsync(() => getConfirmationButton() !== null, 10000000);
    getConfirmationButton()!.click();

    // Wait for the total pinned size to be updated.
    await assertAsync(
        () => this.googleDriveSubpage_?.totalPinnedSize === newSize);
  }
}

class OsSettingsDriver implements OSSettingsDriverInterface {
  private privacyPage(): HTMLElement {
    const privacyPage = querySelectorShadow(document.body, [
      'os-settings-ui',
      'os-settings-main',
      'os-settings-page',
      'os-settings-privacy-page',
    ]);
    assertTrue(privacyPage instanceof HTMLElement);
    return privacyPage;
  }

  // Finds the lock screen settings element. Throws an assertion error if it is
  // not found immediately.
  private lockScreenSettings(): LockScreenSettings {
    const privacyPage = this.privacyPage();
    assertTrue(privacyPage.shadowRoot !== null);

    const lockScreen: Lazy<HTMLElement> = () => {
      assertTrue(privacyPage.shadowRoot !== null);
      const lockScreen =
          privacyPage.shadowRoot.querySelector('settings-lock-screen-subpage');
      assertTrue(lockScreen instanceof HTMLElement);
      return lockScreen;
    };

    // Get the lock screen element once to ensure that it's there, i.e., throw
    // an assertion otherwise.
    lockScreen();

    const passwordDialog: Lazy<HTMLElement|null> = () => {
      assertTrue(privacyPage.shadowRoot !== null);
      return privacyPage.shadowRoot.getElementById('passwordDialog');
    };

    return new LockScreenSettings({lockScreen, passwordDialog});
  }

  async assertOnLockScreenSettings():
      Promise<{lockScreenSettings: LockScreenSettingsRemote}> {
    const lockScreenSettings = await retry(() => this.lockScreenSettings());
    const receiver = new LockScreenSettingsReceiver(lockScreenSettings);
    const remote = receiver.$.bindNewPipeAndPassRemote();

    return {lockScreenSettings: remote};
  }

  async goToLockScreenSettings():
      Promise<{lockScreenSettings: LockScreenSettingsRemote}> {
    const privacyPage = await retry(() => this.privacyPage());
    assertTrue(privacyPage.shadowRoot !== null);

    // Click on button to go to lock screen settings.
    const trigger =
        privacyPage.shadowRoot.getElementById('lockScreenSubpageTrigger');
    assertTrue(trigger !== null);
    trigger.click();

    return await this.assertOnLockScreenSettings();
  }

  private googleDriveSubpage(): SettingsGoogleDriveSubpageElement {
    const googleDriveSubpage = querySelectorShadow(document.body, [
      'os-settings-ui',
      'os-settings-main',
      'os-settings-page',
      'os-settings-files-page',
      'settings-google-drive-subpage',
    ]);
    assertTrue(googleDriveSubpage instanceof HTMLElement);
    return googleDriveSubpage as SettingsGoogleDriveSubpageElement;
  }

  // Finds the google drive settings subpage element.
  private googleDriveSettings(): GoogleDriveSettings {
    const googleDriveSubpage = this.googleDriveSubpage();
    assertTrue(googleDriveSubpage.shadowRoot !== null);
    return new GoogleDriveSettings(googleDriveSubpage);
  }

  // Ensures the page is navigated to the google drive settings.
  async assertOnGoogleDriveSettings():
      Promise<{googleDriveSettings: GoogleDriveSettingsRemote}> {
    const googleDriveSettings = await retry(() => this.googleDriveSettings());
    const receiver = new GoogleDriveSettingsReceiver(googleDriveSettings);
    const remote = receiver.$.bindNewPipeAndPassRemote();
    return {googleDriveSettings: remote};
  }
}

// Passes an OsSettingsDriver remote to the browser process.
export async function register(): Promise<void> {
  const browserProcess = OSSettingsBrowserProcess.getRemote();
  const receiver = new OSSettingsDriverReceiver(new OsSettingsDriver());
  const remote = receiver.$.bindNewPipeAndPassRemote();
  await browserProcess.registerOSSettingsDriver(remote);
}

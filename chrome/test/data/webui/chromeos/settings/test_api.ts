// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';
import 'chrome://os-settings/lazy_load.js';

import {SettingsRadioGroupElement} from 'chrome://os-settings/lazy_load.js';
import {CrButtonElement, SettingsGoogleDriveSubpageElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import {PasswordSettingsApi} from './os_people_page/password_settings_api.js';
import {PinSettingsApi} from './os_people_page/pin_settings_api.js';
import {PasswordSettingsApiRemote} from './password_settings_api.test-mojom-webui.js';
import {PinSettingsApiRemote} from './pin_settings_api.test-mojom-webui.js';
import {GoogleDriveSettingsInterface, GoogleDriveSettingsReceiver, GoogleDriveSettingsRemote, LockScreenSettings_RecoveryDialogAction as RecoveryDialogAction, LockScreenSettingsInterface, LockScreenSettingsReceiver, LockScreenSettingsRemote, OSSettingsBrowserProcess, OSSettingsDriverInterface, OSSettingsDriverReceiver} from './test_api.test-mojom-webui.js';
import {assertAsync, assertForDuration, hasBooleanProperty, hasProperty, hasStringProperty, Lazy, querySelectorShadow, retry, retryUntilSome} from './utils.js';

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
    if (loadTimeData.getBoolean('isAuthPanelEnabled')) {
      const property = () => {
        const authTokenExists =
            hasStringProperty(this.lockScreen, 'authToken') &&
            this.lockScreen['authToken'] !== undefined;
        return isAuthenticated === authTokenExists;
      };

      assertAsync(property);
      assertForDuration(property);
      return;
    }

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

  private queryPasswordSettings(): PasswordSettingsApi|null {
    const el = this.shadowRoot().getElementById('passwordSettings');
    if (!(el instanceof HTMLElement)) {
      return null;
    }
    if (el.hidden) {
      return null;
    }

    return new PasswordSettingsApi(el);
  }

  async assertPasswordControlVisibility(isVisible: boolean): Promise<void> {
    const property = () => {
      const settings = this.queryPasswordSettings();
      return (settings !== null) === isVisible;
    };

    await assertAsync(property);
    await assertForDuration(property);
  }

  async goToPasswordSettings():
      Promise<{passwordSettings: PasswordSettingsApiRemote}> {
    const passwordSettings =
        await retryUntilSome(() => this.queryPasswordSettings());
    return {passwordSettings: passwordSettings.newRemote()};
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
      // Check for presence of "learn more" link
      return toggle.outerHTML.includes('https://support.google.com/chrome') ===
          !isAvailable;
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

  async assertRecoveryControlFocused(): Promise<void> {
    const toggle = await retryUntilSome(() => this.recoveryToggle());
    const isFocused = () => toggle.contains(this.shadowRoot().activeElement);
    await assertAsync(isFocused);
    await assertForDuration(isFocused);
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

  async pinSettingsApi(): Promise<PinSettingsApi> {
    const element = await retryUntilSome(
        () => this.shadowRoot().getElementById('pinSettings'));
    return new PinSettingsApi(element);
  }

  async goToPinSettings(): Promise<{pinSettings: PinSettingsApiRemote}> {
    return {pinSettings: (await this.pinSettingsApi()).newRemote()};
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
    await assertAsync(isFocused);
    await assertForDuration(isFocused);
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

  assertRemainingSpace(freeSpace: string) {
    assertTrue(this.googleDriveSubpage_?.freeSpace === freeSpace);
  }

  async assertBulkPinningSpace(requiredSpace: string, freeSpace: string):
      Promise<void> {
    this.assertRequiredSpace(requiredSpace);
    this.assertRemainingSpace(freeSpace);
  }

  async assertContentCacheSize(contentCacheSize: string): Promise<void> {
    assertTrue(this.googleDriveSubpage_?.contentCacheSize === contentCacheSize);
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
    await assertAsync(() => {
      return this.googleDriveSubpage_?.contentCacheSize === newSize;
    });
  }
}

class OsSettingsDriver implements OSSettingsDriverInterface {
  private privacyPage(): HTMLElement {
    const privacyPage = querySelectorShadow(document.body, [
      'os-settings-ui',
      'os-settings-main',
      'main-page-container',
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
    const trigger = privacyPage.shadowRoot.getElementById('lockScreenRow');
    assertTrue(trigger !== null);
    trigger.click();

    return await this.assertOnLockScreenSettings();
  }

  private googleDriveSubpage(): SettingsGoogleDriveSubpageElement {
    const isRevampWayfindingEnabled =
        loadTimeData.getBoolean('isRevampWayfindingEnabled');

    const elementPath = isRevampWayfindingEnabled ?
        [
          'os-settings-ui',
          'os-settings-main',
          'main-page-container',
          'settings-system-preferences-page',
          'settings-google-drive-subpage',
        ] :
        [
          'os-settings-ui',
          'os-settings-main',
          'main-page-container',
          'os-settings-files-page',
          'settings-google-drive-subpage',
        ];

    const googleDriveSubpage = querySelectorShadow(document.body, elementPath);
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

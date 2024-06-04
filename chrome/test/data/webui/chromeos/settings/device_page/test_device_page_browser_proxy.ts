// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BatteryStatus, DevicePageBrowserProxy, IdleBehavior, LidClosedBehavior, NoteAppInfo, NoteAppLockScreenSupport} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestDevicePageBrowserProxy extends TestBrowserProxy implements
    DevicePageBrowserProxy {
  acIdleBehavior: IdleBehavior|undefined;
  batteryIdleBehavior: IdleBehavior|undefined;
  lastHighlightedDisplayId = '-1';
  lidClosedBehavior: LidClosedBehavior|undefined;
  powerSourceId = '-1';

  private androidAppsReceived_ = false;
  private noteTakingApps_: NoteAppInfo[] = [];
  private hasHapticTouchpad_ = true;
  private hasKeyboard_ = true;
  private hasMouse_ = true;
  private hasPointingStick_ = true;
  private hasTouchpad_ = true;
  private fakeBatteryStatus_: BatteryStatus = {} as BatteryStatus;
  private onNoteTakingAppsUpdated_!:
      (apps: NoteAppInfo[], waitingForAndroid: boolean) => void;

  constructor() {
    super([
      'getStorageEncryptionInfo',
      'requestNoteTakingApps',
      'requestPowerManagementSettings',
      'setPreferredNoteTakingApp',
      'setPreferredNoteTakingAppEnabledOnLockScreen',
      'showShortcutCustomizationApp',
      'showPlayStore',
      'updatePowerStatus',
    ]);
  }

  set hasMouse(hasMouse: boolean) {
    this.hasMouse_ = hasMouse;
    webUIListenerCallback('has-mouse-changed', this.hasMouse_);
  }

  set hasTouchpad(hasTouchpad: boolean) {
    this.hasTouchpad_ = hasTouchpad;
    webUIListenerCallback('has-touchpad-changed', this.hasTouchpad_);
  }

  set hasPointingStick(hasPointingStick: boolean) {
    this.hasPointingStick_ = hasPointingStick;
    webUIListenerCallback('has-pointing-stick-changed', this.hasPointingStick_);
  }

  set hasHapticTouchpad(hasHapticTouchpad: boolean) {
    this.hasHapticTouchpad_ = hasHapticTouchpad;
    webUIListenerCallback(
        'has-haptic-touchpad-changed', this.hasHapticTouchpad_);
  }

  setBatteryStatus(batteryStatus: BatteryStatus): void {
    this.fakeBatteryStatus_ = batteryStatus;
  }

  initializePointers(): void {
    webUIListenerCallback('has-mouse-changed', this.hasMouse_);
    webUIListenerCallback('has-touchpad-changed', this.hasTouchpad_);
    webUIListenerCallback('has-pointing-stick-changed', this.hasPointingStick_);
    webUIListenerCallback(
        'has-haptic-touchpad-changed', this.hasHapticTouchpad_);
  }

  initializeStylus(): void {
    // Enable stylus.
    webUIListenerCallback('has-stylus-changed', true);
  }

  initializeKeyboard(): void {}

  initializeKeyboardWatcher(): void {
    webUIListenerCallback('has-hardware-keyboard', this.hasKeyboard_);
  }

  showShortcutCustomizationApp(): void {
    this.methodCalled('showShortcutCustomizationApp');
  }


  updateAndroidEnabled(): void {}

  updatePowerStatus(): void {
    this.methodCalled('updatePowerStatus');
    webUIListenerCallback('battery-status-changed', this.fakeBatteryStatus_);
  }

  setPowerSource(powerSourceId: string): void {
    this.powerSourceId = powerSourceId;
  }

  requestPowerManagementSettings(): void {
    this.methodCalled('requestPowerManagementSettings');
  }

  setIdleBehavior(behavior: IdleBehavior, whenOnAc: boolean): void {
    if (whenOnAc) {
      this.acIdleBehavior = behavior;
    } else {
      this.batteryIdleBehavior = behavior;
    }
  }

  setLidClosedBehavior(behavior: LidClosedBehavior): void {
    this.lidClosedBehavior = behavior;
  }

  setNoteTakingAppsUpdatedCallback(
      callback: (apps: NoteAppInfo[], waitingForAndroid: boolean) => void):
      void {
    this.onNoteTakingAppsUpdated_ = callback;
  }

  requestNoteTakingApps(): void {
    this.methodCalled('requestNoteTakingApps');
  }

  setPreferredNoteTakingApp(appId: string): void {
    this.methodCalled('setPreferredNoteTakingApp');

    let changed = false;
    this.noteTakingApps_.forEach((app) => {
      changed = changed || app.preferred !== (app.value === appId);
      app.preferred = app.value === appId;
    });

    if (changed) {
      this.scheduleLockScreenAppsUpdated_();
    }
  }

  setPreferredNoteTakingAppEnabledOnLockScreen(enabled: boolean): void {
    this.methodCalled('setPreferredNoteTakingAppEnabledOnLockScreen');

    this.noteTakingApps_.forEach((app) => {
      if (enabled) {
        if (app.preferred) {
          assertEquals(
              NoteAppLockScreenSupport.SUPPORTED, app.lockScreenSupport);
        }
        if (app.lockScreenSupport === NoteAppLockScreenSupport.SUPPORTED) {
          app.lockScreenSupport = NoteAppLockScreenSupport.ENABLED;
        }
      } else {
        if (app.preferred) {
          assertEquals(NoteAppLockScreenSupport.ENABLED, app.lockScreenSupport);
        }
        if (app.lockScreenSupport === NoteAppLockScreenSupport.ENABLED) {
          app.lockScreenSupport = NoteAppLockScreenSupport.SUPPORTED;
        }
      }
    });

    this.scheduleLockScreenAppsUpdated_();
  }

  highlightDisplay(id: string): void {
    this.lastHighlightedDisplayId = id;
  }

  dragDisplayDelta() {}

  openMyFiles() {}

  openBrowsingDataSettings() {}

  setAdaptiveCharging() {}

  setExternalStoragesUpdatedCallback() {}

  updateExternalStorages() {}

  updateStorageInfo() {}

  getStorageEncryptionInfo(): Promise<string> {
    return Promise.resolve('AES-256');
  }

  // Test interface:
  /**
   * Sets whether the app list contains Android apps.
   * @param received Whether the list of Android note-taking apps was received.
   */
  setAndroidAppsReceived(received: boolean): void {
    this.androidAppsReceived_ = received;

    this.scheduleLockScreenAppsUpdated_();
  }

  /**
   * @return App id of the app currently selected as preferred.
   */
  getPreferredNoteTakingAppId(): string {
    const app = this.noteTakingApps_.find((existing) => {
      return existing.preferred;
    });

    return app ? app.value : '';
  }

  /**
   * @return The lock screen support state of the app currently selected as
   *     preferred.
   */
  getPreferredAppLockScreenState(): NoteAppLockScreenSupport|undefined {
    const app = this.noteTakingApps_.find((existing) => {
      return existing.preferred;
    });

    return app?.lockScreenSupport;
  }

  /**
   * Sets the current list of known note taking apps.
   * @param apps The list of apps to set.
   */
  setNoteTakingApps(apps: NoteAppInfo[]): void {
    this.noteTakingApps_ = apps;
    this.scheduleLockScreenAppsUpdated_();
  }

  /**
   * Adds an app to the list of known note-taking apps.
   */
  addNoteTakingApp(app: NoteAppInfo): void {
    const appAlreadyExists = this.noteTakingApps_.find((existing) => {
      return existing.value === app.value;
    });
    assert(!appAlreadyExists);

    this.noteTakingApps_.push(app);
    this.scheduleLockScreenAppsUpdated_();
  }

  /**
   * Invokes the registered note taking apps update callback.
   */
  private scheduleLockScreenAppsUpdated_(): void {
    this.onNoteTakingAppsUpdated_(
        this.noteTakingApps_.map((app) => {
          return Object.assign({}, app);
        }),
        !this.androidAppsReceived_);
  }

  showPlayStore(url: string): void {
    this.methodCalled(this.showPlayStore.name, url);
  }
}

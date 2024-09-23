// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {appParentalControlsHandlerMojom} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

const {PinValidationResult} = appParentalControlsHandlerMojom;
type App = appParentalControlsHandlerMojom.App;
type AppParentalControlsHandlerInterface =
    appParentalControlsHandlerMojom.AppParentalControlsHandlerInterface;
type AppParentalControlsObserverRemoteType =
    appParentalControlsHandlerMojom.AppParentalControlsObserverRemote;
type PinValidationResultType =
    appParentalControlsHandlerMojom.PinValidationResult;

export class FakeAppParentalControlsHandler extends TestBrowserProxy implements
    AppParentalControlsHandlerInterface {
  private apps_: App[] = [];
  private observer_: AppParentalControlsObserverRemoteType|null = null;
  private pin_: string = '';
  private isSetupComplete_: boolean = false;

  constructor() {
    super([
      'getApps',
      'updateApp',
      'addObserver',
      'onControlsDisabled',
      'validatePin',
      'setUpPin',
      'verifyPin',
      'isSetupCompleted',
    ]);
  }

  getApps(): Promise<{apps: App[]}> {
    this.methodCalled('getApps');
    return Promise.resolve({apps: this.apps_});
  }

  updateApp(id: string, isBlocked: boolean): Promise<void> {
    this.methodCalled('updateApp', [id, isBlocked]);
    // Update the state of the app in the local cache.
    for (const app of this.apps_) {
      if (app.id === id) {
        app.isBlocked = isBlocked;
        if (this.observer_) {
          this.observer_.onAppInstalledOrUpdated(app);
        }
      }
    }
    return Promise.resolve();
  }

  onControlsDisabled(): Promise<void> {
    this.methodCalled('onControlsDisabled');
    // Unblock all apps.
    for (const app of this.apps_) {
      app.isBlocked = false;
      if (this.observer_) {
        this.observer_.onAppInstalledOrUpdated(app);
      }
    }
    this.isSetupComplete_ = false;
    return Promise.resolve();
  }

  validatePin(pin: string): Promise<{result: PinValidationResultType}> {
    this.methodCalled('validatePin');
    // Keep these conditions consistent with the production-used implementation
    // in the C++ implementation.
    if (pin.length !== 6) {
      return Promise.resolve({result: PinValidationResult.kPinLengthError});
    }
    if (!this.isNumeric(pin)) {
      return Promise.resolve({result: PinValidationResult.kPinNumericError});
    }
    return Promise.resolve({result: PinValidationResult.kPinValidationSuccess});
  }

  setUpPin(pin: string): Promise<{isSuccess: boolean}> {
    this.methodCalled('setUpPin');
    this.pin_ = pin;
    this.isSetupComplete_ = true;
    return Promise.resolve({isSuccess: true});
  }

  verifyPin(pin: string): Promise<{isSuccess: boolean}> {
    this.methodCalled('verifyPin');
    return Promise.resolve({isSuccess: this.pin_ === pin});
  }

  isSetupCompleted(): Promise<{isCompleted: boolean}> {
    this.methodCalled('isSetupCompleted');
    return Promise.resolve({isCompleted: this.isSetupComplete_});
  }

  addAppForTesting(app: App) {
    this.apps_.push(app);
  }

  addObserver(remoteObserver: AppParentalControlsObserverRemoteType):
      Promise<void> {
    this.methodCalled('addObserver');
    this.observer_ = remoteObserver;
    return Promise.resolve();
  }

  getObserverRemote(): AppParentalControlsObserverRemoteType|null {
    return this.observer_;
  }

  private isNumeric(pin: string): boolean {
    return /^\d+$/.test(pin);
  }
}

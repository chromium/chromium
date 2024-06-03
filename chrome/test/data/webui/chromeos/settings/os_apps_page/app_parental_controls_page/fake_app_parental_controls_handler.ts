// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {appParentalControlsHandlerMojom} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

type App = appParentalControlsHandlerMojom.App;
type AppParentalControlsHandlerInterface =
    appParentalControlsHandlerMojom.AppParentalControlsHandlerInterface;
type AppParentalControlsObserverRemoteType =
    appParentalControlsHandlerMojom.AppParentalControlsObserverRemote;

export class FakeAppParentalControlsHandler extends TestBrowserProxy implements
    AppParentalControlsHandlerInterface {
  private apps_: App[] = [];
  private observer_: AppParentalControlsObserverRemoteType|null = null;

  constructor() {
    super(['getApps', 'updateApp', 'addObserver']);
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
          this.observer_.onReadinessChanged(app);
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
        this.observer_.onReadinessChanged(app);
      }
    }
    return Promise.resolve();
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
}

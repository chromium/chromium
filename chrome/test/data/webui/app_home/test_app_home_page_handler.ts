// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import type {AppInfo, ClickEvent, PageHandlerInterface, PageRemote, RunOnOsLoginMode} from 'chrome://apps/app_home.mojom-webui.js';
import {AppType} from 'chrome://apps/app_home.mojom-webui.js';
import {UserDisplayMode} from 'chrome://apps/user_display_mode.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

interface AppList {
  appList: AppInfo[];
}

export class FakePageHandler extends TestBrowserProxy implements
    PageHandlerInterface {
  private apps_: AppList;
  private callbackRouterRemote_: PageRemote|null = null;

  constructor(apps: AppList) {
    super([
      'uninstallApp',
      'showAppSettings',
      'createAppShortcut',
      'installAppLocally',
      'launchApp',
      'launchDeprecatedAppDialog',
    ]);
    this.apps_ = apps;
  }

  setCallbackRouterRemote(remote: PageRemote) {
    this.callbackRouterRemote_ = remote;
  }

  addAppToList(app: AppInfo) {
    this.apps_.appList.push(app);
  }

  getApps() {
    return Promise.resolve(this.apps_);
  }

  getDeprecationLinkString() {
    let deprecated: string = '';
    for (const app of this.apps_.appList) {
      if (app.appType === AppType.kDeprecatedChromeApp) {
        deprecated = 'Remove X deprecated apps.';
      }
    }
    return Promise.resolve({linkString: deprecated});
  }

  uninstallApp(appId: string) {
    this.methodCalled('uninstallApp', appId);
  }

  showAppSettings(appId: string) {
    this.methodCalled('showAppSettings', appId);
  }

  createAppShortcut(appId: string) {
    this.methodCalled('createAppShortcut', appId);
    return Promise.resolve();
  }

  launchApp(appId: string, clickEvent: ClickEvent) {
    this.methodCalled('launchApp', appId, clickEvent);
    return Promise.resolve();
  }

  setRunOnOsLoginMode(appId: string, runOnOsLoginMode: RunOnOsLoginMode) {
    for (const app of this.apps_.appList) {
      if (app.id === appId) {
        app.runOnOsLoginMode = runOnOsLoginMode;
        this.callbackRouterRemote_!.addApp(app);
        break;
      }
    }
  }

  launchDeprecatedAppDialog() {
    this.methodCalled('launchDeprecatedAppDialog');
  }

  installAppLocally(appId: string) {
    this.methodCalled('installAppLocally', appId);
    for (const app of this.apps_.appList) {
      if (app.id === appId) {
        app.isLocallyInstalled = true;
        this.callbackRouterRemote_!.addApp(app);
        break;
      }
    }
  }

  setUserDisplayMode(appId: string, userDisplayMode: UserDisplayMode) {
    for (const app of this.apps_.appList) {
      if (app.id === appId) {
        app.openInWindow = (userDisplayMode !== UserDisplayMode.kBrowser);
        this.callbackRouterRemote_!.addApp(app);
        break;
      }
    }
  }
}

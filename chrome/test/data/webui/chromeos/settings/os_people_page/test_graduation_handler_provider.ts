// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {graduationHandlerMojom} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

type GraduationHandlerInterface =
    graduationHandlerMojom.GraduationHandlerInterface;

type GraduationObserverRemoteType =
    graduationHandlerMojom.GraduationObserverRemote;

export class TestGraduationHandler extends TestBrowserProxy implements
    GraduationHandlerInterface {
  private observer_: GraduationObserverRemoteType|null = null;

  constructor() {
    super([
      'launchGraduationApp',
      'addObserver',
    ]);
  }

  launchGraduationApp(): void {
    this.methodCalled('launchGraduationApp');
  }

  addObserver(remoteObserver: GraduationObserverRemoteType): Promise<void> {
    this.methodCalled('addObserver');
    this.observer_ = remoteObserver;
    return Promise.resolve();
  }

  getObserverRemote(): GraduationObserverRemoteType|null {
    return this.observer_;
  }
}

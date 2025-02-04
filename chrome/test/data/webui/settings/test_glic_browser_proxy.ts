// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {GlicBrowserProxy} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestGlicBrowserProxy extends TestBrowserProxy implements
    GlicBrowserProxy {
  private glicShortcutResponse_: string = '';

  constructor() {
    super([
      'setGlicOsLauncherEnabled',
      'getGlicShortcut',
      'setGlicShortcut',
      'setShortcutSuspensionState',
    ]);
  }

  override reset() {
    super.reset();
    this.glicShortcutResponse_ = '';
  }

  setGlicOsLauncherEnabled(enabled: boolean) {
    this.methodCalled('setGlicOsLauncherEnabled', enabled);
  }

  setGlicShortcutResponse(response: string) {
    this.glicShortcutResponse_ = response;
  }

  getGlicShortcut() {
    this.methodCalled('getGlicShortcut');
    return Promise.resolve(this.glicShortcutResponse_);
  }

  setGlicShortcut(shortcut: string) {
    this.methodCalled('setGlicShortcut', shortcut);
    return Promise.resolve();
  }

  setShortcutSuspensionState(shouldSuspend: boolean) {
    this.methodCalled('setShortcutSuspensionState', shouldSuspend);
  }
}

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {GlicBrowserProxy} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestGlicBrowserProxy extends TestBrowserProxy implements
    GlicBrowserProxy {
  constructor() {
    super([
      'setGlicOsLauncherEnabled',
      'getGlicShortcut',
      'setGlicShortcut',
      'setShortcutSuspensionState',
    ]);
  }

  setGlicOsLauncherEnabled(enabled: boolean) {
    this.methodCalled('setGlicOsLauncherEnabled', enabled);
  }

  getGlicShortcut() {
    this.methodCalled('getGlicShortcut');
    return Promise.resolve('Ctrl+A');
  }

  setGlicShortcut(shortcut: string) {
    this.methodCalled('setGlicShortcut', shortcut);
  }

  setShortcutSuspensionState(shouldSuspend: boolean) {
    this.methodCalled('setShortcutSuspensionState', shouldSuspend);
  }
}

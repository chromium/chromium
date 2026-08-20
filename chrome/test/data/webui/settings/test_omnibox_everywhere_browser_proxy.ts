// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {OmniboxEverywhereBrowserProxy} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestOmniboxEverywhereBrowserProxy extends TestBrowserProxy
    implements OmniboxEverywhereBrowserProxy {
  private shortcut_: string = 'Alt+Space';

  constructor() {
    super([
      'getOmniboxEverywhereShortcut',
      'setOmniboxEverywhereShortcut',
      'setOmniboxEverywhereShortcutSuspensionState',
    ]);
  }

  setShortcut(shortcut: string) {
    this.shortcut_ = shortcut;
  }

  getOmniboxEverywhereShortcut(): Promise<string> {
    this.methodCalled('getOmniboxEverywhereShortcut');
    return Promise.resolve(this.shortcut_);
  }

  setOmniboxEverywhereShortcut(shortcut: string): Promise<boolean> {
    this.methodCalled('setOmniboxEverywhereShortcut', shortcut);
    this.shortcut_ = shortcut;
    return Promise.resolve(true);
  }

  setOmniboxEverywhereShortcutSuspensionState(shouldSuspend: boolean) {
    this.methodCalled(
        'setOmniboxEverywhereShortcutSuspensionState', shouldSuspend);
  }
}

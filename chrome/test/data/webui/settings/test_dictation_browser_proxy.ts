// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DictationBrowserProxy} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestDictationBrowserProxy extends TestBrowserProxy implements
    DictationBrowserProxy {
  private setDictationShortcutResult_: boolean = true;

  constructor() {
    super([
      'setDictationShortcut',
    ]);
  }

  setDictationShortcut(shortcut: string): Promise<boolean> {
    this.methodCalled('setDictationShortcut', shortcut);
    return Promise.resolve(this.setDictationShortcutResult_);
  }

  setDictationShortcutResult(result: boolean) {
    this.setDictationShortcutResult_ = result;
  }
}

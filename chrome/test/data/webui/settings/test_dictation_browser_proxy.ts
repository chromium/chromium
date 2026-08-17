// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DictationBrowserProxy} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestDictationBrowserProxy extends TestBrowserProxy implements
    DictationBrowserProxy {
  private setDictationShortcutResult_: boolean = true;
  private dictationShortcut_: string = '';

  constructor() {
    super([
      'getDictationShortcut',
      'setDictationShortcut',
    ]);
  }

  getDictationShortcut(): Promise<string> {
    this.methodCalled('getDictationShortcut');
    return Promise.resolve(this.dictationShortcut_);
  }

  setDictationShortcut(shortcut: string): Promise<boolean> {
    this.methodCalled('setDictationShortcut', shortcut);
    if (this.setDictationShortcutResult_) {
      this.dictationShortcut_ = shortcut;
    }
    return Promise.resolve(this.setDictationShortcutResult_);
  }

  setDictationShortcutResult(result: boolean) {
    this.setDictationShortcutResult_ = result;
  }

  setDictationShortcutValue(shortcut: string) {
    this.dictationShortcut_ = shortcut;
  }
}

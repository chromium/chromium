// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ThemeObserverInterface, ThemeObserverRemote, ThemeProviderInterface} from 'chrome://personalization/trusted/personalization_app.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestThemeProvider extends
    TestBrowserProxy<ThemeProviderInterface> implements ThemeProviderInterface {
  constructor() {
    super([
      'setThemeObserver',
      'setColorModePref',
    ]);
  }

  themeObserverRemote: ThemeObserverInterface|null = null;

  setThemeObserver(remote: ThemeObserverRemote) {
    this.methodCalled('setThemeObserver');
    this.themeObserverRemote = remote;
    window.setTimeout(() => {
      this.themeObserverRemote!.onColorModeChanged(/*darkModeEnabled=*/ true);
    }, 0);
  }

  setColorModePref(darkModeEnabled: boolean) {
    this.methodCalled('setColorModePref', darkModeEnabled);
  }
}

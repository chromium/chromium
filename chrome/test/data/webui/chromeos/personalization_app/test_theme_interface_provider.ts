// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ColorScheme, ThemeObserverInterface, ThemeObserverRemote, ThemeProviderInterface} from 'chrome://personalization/js/personalization_app.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestThemeProvider extends TestBrowserProxy implements
    ThemeProviderInterface {
  constructor() {
    super([
      'setThemeObserver',
      'setColorModePref',
      'setColorModeAutoScheduleEnabled',
      'setColorScheme',
      'setStaticColor',
      'isDarkModeEnabled',
      'isColorModeAutoScheduleEnabled',
    ]);
  }

  isDarkModeEnabledResponse = true;
  isColorModeAutoScheduleEnabledResponse = true;

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

  setColorModeAutoScheduleEnabled(enabled: boolean) {
    this.methodCalled('setColorModeAutoScheduleEnabled', enabled);
  }

  setColorScheme(colorScheme: ColorScheme) {
    this.methodCalled('setColorScheme', colorScheme);
  }

  setStaticColor(color: SkColor) {
    this.methodCalled('setStaticColor', color);
  }

  isDarkModeEnabled() {
    this.methodCalled('isDarkModeEnabled');
    return Promise.resolve({darkModeEnabled: this.isDarkModeEnabledResponse});
  }

  isColorModeAutoScheduleEnabled() {
    this.methodCalled('isColorModeAutoScheduleEnabled');
    return Promise.resolve(
        {enabled: this.isColorModeAutoScheduleEnabledResponse});
  }
}

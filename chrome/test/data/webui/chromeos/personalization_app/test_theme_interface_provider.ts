// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ColorScheme, ThemeObserverInterface, ThemeObserverRemote, ThemeProviderInterface} from 'chrome://personalization/js/personalization_app.js';
import {hexColorToSkColor} from 'chrome://resources/js/color_utils.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestThemeProvider extends TestBrowserProxy implements
    ThemeProviderInterface {
  constructor() {
    super([
      'setThemeObserver',
      'setColorModePref',
      'setColorModeAutoScheduleEnabled',
      'enableGeolocationForSystemServices',
      'setColorScheme',
      'setStaticColor',
      'generateSampleColorSchemes',
      'getColorScheme',
      'getStaticColor',
      'isDarkModeEnabled',
      'isColorModeAutoScheduleEnabled',
      'isGeolocationEnabledForSystemServices',
      'isGeolocationUserModifiable',
      'getSunriseTime',
      'getSunsetTime',
    ]);
    this.staticColor = null;
  }

  isDarkModeEnabledResponse = true;
  isColorModeAutoScheduleEnabledResponse = true;
  isGeolocationPermissionEnabledResponse = true;
  isGeolocationUserModifiableResponse = true;

  staticColor: SkColor|null;
  colorScheme = ColorScheme.kTonalSpot;

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

  enableGeolocationForSystemServices() {
    this.methodCalled('enableGeolocationForSystemServices');
  }

  setColorScheme(colorScheme: ColorScheme) {
    this.methodCalled('setColorScheme', colorScheme);
    this.staticColor = null;
    this.colorScheme = colorScheme;
  }

  setStaticColor(color: SkColor) {
    this.methodCalled('setStaticColor', color);
    this.staticColor = color;
    this.colorScheme = ColorScheme.kStatic;
  }

  getColorScheme() {
    this.methodCalled('getColorScheme');
    return Promise.resolve({colorScheme: this.colorScheme});
  }

  getStaticColor() {
    this.methodCalled('getStaticColor');
    return Promise.resolve({staticColor: this.staticColor});
  }

  generateSampleColorSchemes() {
    this.methodCalled('generateSampleColorSchemes');
    const sampleColorSchemes = [
      ColorScheme.kTonalSpot,
      ColorScheme.kExpressive,
      ColorScheme.kNeutral,
      ColorScheme.kVibrant,
    ].map((colorScheme) => {
      return {
        scheme: colorScheme,
        primary: hexColorToSkColor('#ffffff'),
        secondary: hexColorToSkColor('#ffffff'),
        tertiary: hexColorToSkColor('#ffffff'),
      };
    });
    return Promise.resolve({sampleColorSchemes});
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

  isGeolocationEnabledForSystemServices() {
    this.methodCalled('isGeolocationEnabledForSystemServices');
    return Promise.resolve(
        {geolocationEnabled: this.isGeolocationPermissionEnabledResponse});
  }

  isGeolocationUserModifiable() {
    this.methodCalled('isGeolocationUserModifiable');
    return Promise.resolve({
      geolocationIsUserModifiable: this.isGeolocationUserModifiableResponse,
    });
  }
}

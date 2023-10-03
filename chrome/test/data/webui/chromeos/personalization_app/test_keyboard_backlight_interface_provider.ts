// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BacklightColor, CurrentBacklightState, KeyboardBacklightObserverInterface, KeyboardBacklightObserverRemote, KeyboardBacklightProviderInterface} from 'chrome://personalization/js/personalization_app.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestKeyboardBacklightProvider extends TestBrowserProxy implements
    KeyboardBacklightProviderInterface {
  zoneCount: number = 5;
  zoneColors: BacklightColor[] = [
    BacklightColor.kBlue,
    BacklightColor.kRed,
    BacklightColor.kWallpaper,
    BacklightColor.kYellow,
  ];
  currentBacklightState: CurrentBacklightState = {color: BacklightColor.kBlue};

  constructor() {
    super([
      'setKeyboardBacklightObserver',
      'setBacklightColor',
      'setBacklightZoneColor',
      'shouldShowNudge',
      'handleNudgeShown',
    ]);
  }

  keyboardBacklightObserverRemote: KeyboardBacklightObserverInterface|null =
      null;

  setZoneCount(zoneCount: number) {
    this.zoneCount = zoneCount;
  }

  setCurrentBacklightState(backlightState: CurrentBacklightState) {
    this.currentBacklightState = backlightState;
  }

  setBacklightColor(backlightColor: BacklightColor) {
    this.methodCalled('setBacklightColor', backlightColor);
  }

  setBacklightZoneColor(zone: number, backlightColor: BacklightColor) {
    this.methodCalled('setBacklightZoneColor', zone, backlightColor);
  }

  shouldShowNudge() {
    this.methodCalled('shouldShowNudge');
    return Promise.resolve({shouldShowNudge: true});
  }

  handleNudgeShown() {
    this.methodCalled('handleNudgeShown');
  }

  setKeyboardBacklightObserver(remote: KeyboardBacklightObserverRemote) {
    this.methodCalled('setKeyboardBacklightObserver', remote);
    this.keyboardBacklightObserverRemote = remote;
  }

  fireOnBacklightStateChanged(currentBacklightState: CurrentBacklightState) {
    this.keyboardBacklightObserverRemote!.onBacklightStateChanged(
        currentBacklightState);
  }

  fireOnWallpaperColorChanged(wallpaperColor: SkColor) {
    this.keyboardBacklightObserverRemote!.onWallpaperColorChanged(
        wallpaperColor);
  }
}

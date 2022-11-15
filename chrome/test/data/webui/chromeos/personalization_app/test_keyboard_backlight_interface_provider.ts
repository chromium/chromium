// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BacklightColor, KeyboardBacklightObserverInterface, KeyboardBacklightObserverRemote, KeyboardBacklightProviderInterface} from 'chrome://personalization/js/personalization_app.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestKeyboardBacklightProvider extends TestBrowserProxy implements
    KeyboardBacklightProviderInterface {
  public backlightColor: BacklightColor = BacklightColor.kBlue;

  constructor() {
    super([
      'setKeyboardBacklightObserver',
      'setBacklightColor',
      'shouldShowNudge',
      'handleNudgeShown',
    ]);
  }

  keyboardBacklightObserverRemote: KeyboardBacklightObserverInterface|null =
      null;

  setBacklightColor(backlightColor: BacklightColor) {
    this.methodCalled('setBacklightColor', backlightColor);
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

  fireOnBacklightColorChanged(backlightColor: BacklightColor) {
    this.keyboardBacklightObserverRemote!.onBacklightColorChanged(
        backlightColor);
  }

  fireOnWallpaperColorChanged(wallpaperColor: SkColor) {
    this.keyboardBacklightObserverRemote!.onWallpaperColorChanged(
        wallpaperColor);
  }
}

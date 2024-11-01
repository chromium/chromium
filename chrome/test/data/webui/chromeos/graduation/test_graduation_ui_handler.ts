// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AuthResult, GraduationScreen, GraduationUiHandlerInterface, ProfileInfo} from 'chrome://graduation/mojom/graduation_ui.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestGraduationUiHandler extends TestBrowserProxy implements
    GraduationUiHandlerInterface {
  private authResult: AuthResult;
  private email: string;
  private photoUrl: string;
  private profileInfo: ProfileInfo;
  private lastScreen: GraduationScreen;

  constructor() {
    super([
      'authenticateWebview',
      'getProfileInfo',
      'onScreenSwitched',
      'onTransferComplete',
    ]);
    this.email = 'user1test@gmail.com';
    // Evaluates to a square solid black icon.
    this.photoUrl = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQ'
    'AAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=';
    this.profileInfo = this.buildTestProfileInfo();
    this.authResult = AuthResult.kSuccess;
    this.lastScreen = GraduationScreen.kWelcome;
  }

  authenticateWebview(): Promise<{result: AuthResult}> {
    this.methodCalled('authenticateWebview');
    return Promise.resolve({result: this.authResult});
  }

  getProfileInfo(): Promise<{profileInfo: ProfileInfo}> {
    this.methodCalled('getProfileInfo');
    return Promise.resolve({profileInfo: this.profileInfo!});
  }

  onScreenSwitched(graduationScreen: GraduationScreen): Promise<void> {
    this.lastScreen = graduationScreen;
    this.methodCalled('onScreenSwitched', graduationScreen);
    return Promise.resolve();
  }

  onTransferComplete(): Promise<void> {
    this.methodCalled('onTransferComplete');
    return Promise.resolve();
  }

  setAuthResult(authResult: AuthResult): void {
    this.authResult = authResult;
  }

  getLastScreen(): GraduationScreen {
    return this.lastScreen;
  }

  private buildTestProfileInfo(): ProfileInfo {
    const profileInfo: ProfileInfo = {
      email: this.email,
      photoUrl: this.photoUrl,
    };
    return profileInfo;
  }
}

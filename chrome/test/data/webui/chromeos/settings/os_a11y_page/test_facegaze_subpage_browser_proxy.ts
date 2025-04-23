// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FaceGazeSubpageBrowserProxy} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestFaceGazeSubpageBrowserProxy extends TestBrowserProxy implements
    FaceGazeSubpageBrowserProxy {
  constructor() {
    super([
      'requestEnableFaceGaze',
      'toggleGestureInfoForSettings',
    ]);
  }

  requestEnableFaceGaze(enable: boolean): void {
    this.methodCalled('requestEnableFaceGaze', [enable]);
  }

  toggleGestureInfoForSettings(enabled: boolean): void {
    this.methodCalled('toggleGestureInfoForSettings', [enabled]);
  }
}

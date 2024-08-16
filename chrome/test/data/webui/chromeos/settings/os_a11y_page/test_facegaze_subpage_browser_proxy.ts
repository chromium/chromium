// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FaceGazeSubpageBrowserProxy} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestFaceGazeSubpageBrowserProxy extends TestBrowserProxy implements
    FaceGazeSubpageBrowserProxy {
  constructor() {
    super([
      'toggleGestureInfoForSettings',
    ]);
  }

  toggleGestureInfoForSettings(enabled: boolean): void {
    this.methodCalled('toggleGestureInfoForSettings', [enabled]);
  }
}

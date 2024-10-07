// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {graduationHandlerMojom} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

type GraduationHandlerInterface =
    graduationHandlerMojom.GraduationHandlerInterface;

export class TestGraduationHandler extends TestBrowserProxy implements
    GraduationHandlerInterface {
  constructor() {
    super([
      'launchGraduationApp',
    ]);
  }

  launchGraduationApp(): void {
    this.methodCalled('launchGraduationApp');
  }
}
